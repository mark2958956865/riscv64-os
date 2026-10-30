CROSS_COMPILE := riscv64-unknown-elf-

CC := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy

CFLAGS := -march=rv64imac_zicsr -mabi=lp64 \
          -mcmodel=medany -ffreestanding \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -nostdlib

OBJS := entry.o kerneltrap.o switch.o sbi.o main.o printf.o \
        trap.o task.o app.o syscall.o string.o

all: os.bin

os.elf: $(OBJS) os.ld
	$(CC) $(CFLAGS) -nostartfiles -static \
		-Wl,-Map=os.map -Wl,--build-id=none -T os.ld \
		$(OBJS) -o $@

os.bin: os.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean

clean:
	rm -f *.o *.elf *.bin