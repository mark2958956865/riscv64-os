.DEFAULT_GOAL := all

# RISC-V 交叉编译工具链
CROSS_COMPILE := riscv64-unknown-elf-

CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
HOSTCC := gcc

# 头文件路径
INCLUDE := -I./include -I./include/timeros

# 编译选项
CFLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 \
          -mcmodel=medany -ffreestanding \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -nostdlib \
          -g -MMD -MP \
          $(INCLUDE)

# 内核目标文件（保持原有配置）
OBJS := entry.o kerneltrap.o switch.o \
        sbi.o main.o printk.o trap.o \
        task.o app.o syscall.o string.o \
        timer.o vsprintf.o stack.o address.o \
        assert.o printf.o link_app.o loader.o

# 用户程序（与 user/Makefile 保持一致）
USER_APPS := initproc time user_shell write xec

USER_BINS := $(addprefix user/bin/,$(USER_APPS))

# 用户态公共库
USER_LIB_SRCS := lib/app.c \
                 lib/printf.c \
                 lib/vsprintf.c \
                 lib/string.c

.PHONY: all build_app clean clean_all

# 默认生成完整的内核镜像
all: os.bin

# 编译宿主机 ELF 打包工具
build.out: build.c
	$(HOSTCC) -O2 $< -o $@

# 当用户程序或其依赖改变时重新编译
user/bin/%: user/%.c user/user.ld \
            user/Makefile $(USER_LIB_SRCS)
	$(MAKE) -C user bin/$*

# 打包应用程序，生成 link_app.S
src/link_app.S: build.out $(USER_BINS)
	rm -f user/bin/*.map
	./build.out
	
# 提供单独打包目标
build_app: src/link_app.S

# 编译应用程序打包汇编文件
link_app.o: src/link_app.S
	$(CC) $(CFLAGS) -c $< -o $@

# 链接内核
os.elf: $(OBJS) os.ld
	$(CC) $(CFLAGS) -nostartfiles -static \
	    -Wl,-Map=os.map -Wl,--build-id=none \
	    -T os.ld $(OBJS) -o $@

# 生成二进制内核镜像
os.bin: os.elf
	$(OBJCOPY) -O binary $< $@

# 编译 src 中的 C 文件
%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 编译 lib 中的 C 文件
%.o: lib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 编译 src 中的汇编文件
%.o: src/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# 自动读取头文件依赖
-include $(OBJS:.o=.d)

# 清理内核编译结果
clean:
	rm -f *.o *.d *.elf *.bin *.map build.out

# 清理整个 OS 的构建产物
clean_all: clean
	$(MAKE) -C user clean
	rm -f src/link_app.S
