CROSS_COMPILE := riscv64-unknown-elf-

CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy

INCLUDE := -I./include -I./include/timeros

CFLAGS := -march=rv64imac_zicsr -mabi=lp64 \
          -mcmodel=medany -ffreestanding \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -nostdlib \
          $(INCLUDE)

CFLAGS += -g

OBJS := entry.o kerneltrap.o switch.o \
        sbi.o main.o printk.o trap.o \
        task.o app.o syscall.o string.o timer.o vsprintf.o stack.o address.o assert.o printf.o

all: os.bin

os.elf: $(OBJS) os.ld
	$(CC) $(CFLAGS) -nostartfiles -static \
		-Wl,-Map=os.map -Wl,--build-id=none \
		-T os.ld $(OBJS) -o $@

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

.PHONY: all clean

clean:
	rm -f *.o *.elf *.bin *.map