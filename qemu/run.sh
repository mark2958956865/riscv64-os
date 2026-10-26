
#!/bin/bash
set -e

# 获取当前脚本目录
SHELL_FOLDER=$(cd "$(dirname "$0")"; pwd)

# QEMU 可执行文件
QEMU="$SHELL_FOLDER/build/qemu-system-riscv64"

# 合成后的 Flash 镜像
FLASH_IMG="$SHELL_FOLDER/build/boot/flash.img"

# 检查文件
if [ ! -x "$QEMU" ]; then
    echo "Error: QEMU executable not found."
    exit 1
fi

if [ ! -f "$FLASH_IMG" ]; then
    echo "Error: Flash image not found."
    exit 1
fi

# 启动 Quard Star
"$QEMU" \
    -M quard-star \
    -m 1G \
    -smp 8 \
    -bios none \
    -drive if=pflash,unit=0,format=raw,file="$FLASH_IMG" \
    -d in_asm,int,guest_errors \
    -D "$SHELL_FOLDER/qemu.log" \
    -display none \
    -serial stdio \
    -serial null \
    -serial file:"$SHELL_FOLDER/uart2.log" \
    -monitor none
