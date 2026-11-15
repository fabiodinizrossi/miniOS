CC := gcc
LD := ld
QEMU ?= qemu-system-i386
CFLAGS := -m32 -std=c11 -O2 -Wall -Wextra -Werror -ffreestanding \
          -fno-builtin -fno-pie -fno-pic -fno-stack-protector \
          -fno-asynchronous-unwind-tables -fno-unwind-tables \
          -mno-sse -mno-mmx -msoft-float

.PHONY: all check iso run run-direct run-terminal clean
all: build/kernel.elf
build:
	mkdir -p build
build/boot.o: src/boot.S | build
	$(CC) -m32 -ffreestanding -fno-pie -c $< -o $@
build/kernel.o: src/kernel.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/kernel.elf: build/boot.o build/kernel.o linker.ld
	$(LD) -m elf_i386 -T linker.ld -o $@ build/boot.o build/kernel.o
check: build/kernel.elf
	python3 tools/check_elf.py $<
iso: check
	command -v grub-mkrescue >/dev/null
	command -v xorriso >/dev/null
	grub-file --is-x86-multiboot build/kernel.elf
	mkdir -p build/isodir/boot/grub
	cp build/kernel.elf build/isodir/boot/kernel.elf
	cp boot/grub/grub.cfg build/isodir/boot/grub/grub.cfg
	grub-mkrescue -o build/minios.iso build/isodir
run: iso
	$(QEMU) -accel tcg -m 64M -cdrom build/minios.iso -boot d -no-reboot -no-shutdown -serial stdio
run-direct: check
	$(QEMU) -accel tcg -m 64M -kernel build/kernel.elf -no-reboot -no-shutdown -serial stdio
run-terminal: check
	$(QEMU) -accel tcg -m 64M -kernel build/kernel.elf -display none -monitor none -serial stdio -no-reboot -no-shutdown
clean:
	rm -rf build
