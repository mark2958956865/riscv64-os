
#!/bin/bash
set -e

# 获取当前脚本所在目录（QEMU 源码根目录）
SHELL_FOLDER=$(cd "$(dirname "$0")"; pwd)

cd "$SHELL_FOLDER"

# ==========================================
# 1. 编译 QEMU
# ==========================================

if [ ! -f build/build.ninja ]; then
    mkdir -p build
    cd build

    ../configure \
        --target-list=riscv64-softmmu \
        --disable-docs \
        --disable-rust

    cd ..
fi

ninja -C build -j2 qemu-system-riscv64


# ==========================================
# 2. 配置 RISC-V 交叉编译工具链
# ==========================================

export PATH="$HOME/tools/riscv/riscv/bin:$PATH"

CROSS_PREFIX="riscv64-unknown-elf-"

# OpenSBI 位于 QEMU 的上一级目录
OPENSBI_DIR="$SHELL_FOLDER/../opensbi-1.2"

mkdir -p build/boot


# ==========================================
# 3. 编译 Flash 启动程序 start.S
# ==========================================

riscv64-unknown-elf-gcc \
    -march=rv64imac_zicsr_zifencei \
    -mabi=lp64 \
    -nostdlib \
    -nostartfiles \
    -fno-pic \
    -fno-pie \
    -no-pie \
    -Wl,--build-id=none \
    -T boot/boot.lds \
    boot/start.S \
    -o build/boot/boot.elf

# ELF 转换为纯二进制
riscv64-unknown-elf-objcopy \
    -O binary \
    build/boot/boot.elf \
    build/boot/boot.bin


# ==========================================
# 4. 编译 OpenSBI 1.2
# ==========================================

echo "Building OpenSBI..."

make -C "$OPENSBI_DIR" \
    CROSS_COMPILE="$CROSS_PREFIX" \
    PLATFORM=quard_star \
    -j2

OPENSBI_BIN="$OPENSBI_DIR/build/platform/quard_star/firmware/fw_jump.bin"


# ==========================================
# 5. 编译设备树 DTS -> DTB
# ==========================================

echo "Building device tree..."

dtc -I dts -O dtb \
    -o build/boot/quard_star_sbi.dtb \
    "$OPENSBI_DIR/platform/quard_star/dts/quard_star_sbi.dts"


# ==========================================
# 6. 检查固件大小
# ==========================================

BOOT_BIN="build/boot/boot.bin"
DTB_BIN="build/boot/quard_star_sbi.dtb"
FLASH_IMG="build/boot/flash.img"

# Bootloader 最多占 512 KiB
test "$(stat -c%s "$BOOT_BIN")" -le 524288

# start.S 固定复制 512 KiB 的 DTB 区域
test "$(stat -c%s "$DTB_BIN")" -le 524288

# start.S 固定复制 2 MiB 的 OpenSBI 区域
test "$(stat -c%s "$OPENSBI_BIN")" -le 2097152


# ==========================================
# 7. 合成 32 MiB Flash 镜像
# ==========================================

echo "Creating Flash image..."

# 创建并清空 32 MiB Flash 镜像
dd if=/dev/zero \
    of="$FLASH_IMG" \
    bs=1M count=32 status=none

# 写入 Bootloader，Flash 偏移 0
dd if="$BOOT_BIN" \
    of="$FLASH_IMG" \
    bs=1K seek=0 \
    conv=notrunc status=none

# 写入设备树，Flash 偏移 0x80000
dd if="$DTB_BIN" \
    of="$FLASH_IMG" \
    bs=1K seek=512 \
    conv=notrunc status=none

# 写入 OpenSBI，Flash 偏移 0x200000
dd if="$OPENSBI_BIN" \
    of="$FLASH_IMG" \
    bs=1K seek=2048 \
    conv=notrunc status=none


# ==========================================
# 8. 编译完成
# ==========================================

echo "QEMU, OpenSBI and firmware build completed."
echo "Flash image: $SHELL_FOLDER/$FLASH_IMG"
