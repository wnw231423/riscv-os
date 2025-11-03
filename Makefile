K=kernel
INC=include

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -I$(INC)

# Disable PIE when possible
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

MODULES = $(shell ls -d $(K)/*)

OBJS = $(foreach module, $(MODULES), \
		$(patsubst %.c, %.o, $(wildcard $(module)/*.c)) \
		$(patsubst %.S, %.o, $(wildcard $(module)/*.S)))
OBJS := $(filter-out $K/proc/initcode.o, $(OBJS))

%.o: %.S
	$(CC) -g -c -o $@ $<

# for lab4
INIT: $K/proc/initcode.c
	$(CC) $(CFLAGS) -I . -march=rv64g -nostdinc -c $K/proc/initcode.c -o $K/proc/initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $K/proc/initcode.out $K/proc/initcode.o
	$(OBJCOPY) -S -O binary $K/proc/initcode.out $K/proc/initcode
	xxd -i $K/proc/initcode > $(INC)/proc/initcode.h
	rm -f $K/proc/initcode $K/proc/initcode.d $K/proc/initcode.o $K/proc/initcode.out

$K/kernel: INIT $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

# QEMU选项
CPUNUM = 3
QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUNUM) -nographic

qemu: $K/kernel
	$(QEMU) $(QEMUOPTS)

# 清理规则
clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	*/*.o */*.d */*.asm */*.sym */*/*.o */*/*.d */*/*.asm */*/*.sym \
	$K/kernel .gdbinit

# 调试相关
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

-include kernel/*.d
