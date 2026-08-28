######################################################################
# @author      : Xiaotian Wu (yetist@gmail.com)
# @file        : Makefile
# @created     : 星期三 8月 26, 2026 20:10:04 CST
######################################################################

# 交叉编译链前缀
CROSS_COMPILE ?= loongarch64-linux-gnu-
QEMU          := qemu-system-loongarch64

# 编译器及工具定义
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
AS      := $(CROSS_COMPILE)as
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump

MCFLAGS := -march=loongarch64 -mabi=lp64s -fno-stack-protector -mstrict-align
CFLAGS  := -g -ggdb -Wall -O2 -ffreestanding $(MCFLAGS) -fno-pie
#LDFLAGS := -T board/$(BOARD)/board.ld -nostdlib -no-pie -fno-builtin -Wl,--gc-sections -Wl,--no-warn-rwx-segments

#CFLAGS  := -g -ggdb -Wall -O0 -fno-stack-protector -ffreestanding -nostdlib -fno-builtin -std=gnu99 -march=loongarch64 -mabi=lp64s
ASFLAGS := -g -ggdb -Wall -O0 $(MCFLAGS) -fno-pie

ASFLAGS += -ffunction-sections -fdata-sections -ffreestanding -std=gnu99
CFLAGS		+=	-ffunction-sections -fdata-sections -ffreestanding -std=gnu99 $(DEFINES)
LDFLAGS		+=	-Wl,-gc-sections

LDFLAGS := -Wl,-gc-sections -nostdlib -no-pie -fno-builtin -Wl,--gc-sections -Wl,--no-warn-rwx-segments

#CFLAGS="-std=c11 -O2 -g3 -Wall -Wextra --target=loongarch64-unknown-elf -fuse-ld=lld -fno-stack-protector -ffreestanding -nostdlib"

## Build the shell.
#$CC $CFLAGS -Wl,-Tuser.ld -Wl,-Map=shell.map -o shell.elf shell.c user.c common.c
#
#
#$OBJCOPY -Ibinary -Oelf32-littleriscv shell.bin shell.bin.o
#
## Build the kernel.
#$CC $CFLAGS -Wl,-Tkernel.ld -Wl,-Map=kernel.map -o kernel.elf \
#  kernel.c common.c shell.bin.o
#
all: shell.bin kernel.bin

shell.elf: shell.o user.o common.o start.o
	echo $(LD) -T user.ld -o $@ $^
	$(LD) -T user.ld -o $@ $^

kernel.elf: kernel.o common.o boot.o shell.bin.o
	$(LD) -T kernel.ld -o $@ $^

shell.bin.o: shell.bin
	$(OBJCOPY) -Ibinary -Oelf64-loongarch shell.bin shell.bin.o

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

%.o : %.S
	@echo [AS] $<
	$(CC) $(ASFLAGS) -MD -MP -MF $@.d -c $< -o $@

%.o : %.c
	@echo [CC] $<
	$(CC) $(CFLAGS) -MD -MP -MF $@.d -c $< -o $@

run: $(X_ELF)
	$(QEMU) \
		-m 1G \
		-cpu la464 \
		-machine virt \
		-smp 1 \
		-nographic \
		-kernel kernel.elf

gdbrun: $(X_ELF)
	$(QEMU) \
		-m 1G \
		-cpu la464 \
		-machine virt \
		-smp 1 -nographic \
		-S -gdb tcp::1234,ipv4 \
		-rtc base=localtime \
		-kernel kernel.elf

.PHONY: clean

clean:
	rm -f *.o *.bin *.elf *.o.d *.map
