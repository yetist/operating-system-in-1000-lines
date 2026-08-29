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

MCFLAGS := -march=loongarch64 -mabi=lp64s -fno-stack-protector -ffreestanding -mstrict-align \
		   -fno-pie -ffunction-sections -fdata-sections -std=gnu99
CFLAGS  := -g -ggdb -Wall -Wextra -O2 $(MCFLAGS)
ASFLAGS := -g -ggdb -Wall -Wextra -O0 $(MCFLAGS)
LDFLAGS := -nostdlib -fno-builtin -no-pie -Wl,-gc-sections -Wl,--no-warn-rwx-segments

all: shell.bin kernel.bin

shell.elf: shell.o user.o common.o start.o
	@echo [LD] $@
	@$(LD) -T user.ld -o $@ $^
	@$(OBJDUMP) -d $@ > shell.objdump

kernel.elf: kernel.o common.o boot.o shell.bin.o
	@echo [LD] $@
	@$(LD) -T kernel.ld -o $@ $^
	@$(OBJDUMP) -d $@ > kernel.objdump

shell.bin.o: shell.bin
	@$(OBJCOPY) -Ibinary -Oelf64-loongarch shell.bin shell.bin.o

%.bin: %.elf
	@echo [OBJCOPY] $@
	@$(OBJCOPY) -O binary $< $@

%.o : %.S
	@echo [AS] $<
	@$(CC) $(ASFLAGS) -MD -MP -MF $@.d -c $< -o $@

%.o : %.c
	@echo [CC] $<
	@$(CC) $(CFLAGS) -MD -MP -MF $@.d -c $< -o $@

disk.tar:
	(cd disk && tar cf ../disk.tar --format=ustar *.txt)

run: kernel.elf disk.tar
	@$(QEMU) \
		-m 1G \
		-cpu la464 \
		-machine virt \
		-smp 1 \
		-nographic \
		-d unimp,guest_errors,int,cpu_reset \
		-D qemu.log \
		-drive id=drive0,file=disk.tar,format=raw,if=none \
		-device virtio-blk-pci,drive=drive0 \
		-rtc base=localtime \
		-kernel kernel.elf

debug: kernel.elf disk.tar
	@$(QEMU) \
		-m 1G \
		-cpu la464 \
		-machine virt \
		-smp 1 \
		-nographic \
		-d unimp,guest_errors,int,cpu_reset \
		-D qemu.log \
		-drive id=drive0,file=disk.tar,format=raw,if=none \
		-device virtio-blk-pci,drive=drive0 \
		-S -gdb tcp::1234,ipv4 \
		-rtc base=localtime \
		-kernel kernel.elf

.PHONY: clean

clean:
	rm -f *.o *.bin *.elf *.o.d *.map
