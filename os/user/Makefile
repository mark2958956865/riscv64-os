.DEFAULT_GOAL := all

# RISC-V 交叉编译工具链
CROSS_COMPILE := riscv64-unknown-elf-
CC := $(CROSS_COMPILE)gcc

# 目录配置
BIN_DIR := bin
INCLUDE := -I../include -I../include/timeros

# 编译选项
CFLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 \
          -mcmodel=medany -ffreestanding \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -nostdlib -g \
          $(INCLUDE)

# 链接选项
LDFLAGS := -nostdlib -nostartfiles -static \
           -T user.ld -Wl,--build-id=none

# 用户态公共库
LIB_SRCS := ../lib/app.c \
            ../lib/printf.c \
            ../lib/vsprintf.c \
            ../lib/string.c

# 所有用户程序
APPS := initproc time user_shell write xec
TARGETS := $(addprefix $(BIN_DIR)/,$(APPS))

.PHONY: all clean $(APPS)

# 编译所有用户程序
all: $(TARGETS)

# 兼容 make time、make write 等命令
$(APPS): %: $(BIN_DIR)/%

# 创建输出目录
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# 统一编译用户程序并链接公共库
$(BIN_DIR)/%: %.c $(LIB_SRCS) user.ld | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	    -Wl,-Map=$@.map \
	    -o $@ $< $(LIB_SRCS)

# 清理所有用户程序
clean:
	rm -f $(BIN_DIR)/*
