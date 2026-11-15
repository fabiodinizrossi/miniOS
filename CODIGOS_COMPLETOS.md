# MiniOS — códigos completos

Todos os arquivos abaixo estão prontos na estrutura do projeto. Consulte README.md para instalação e execução.

## src/boot.S

```asm
/* GNU assembler, sintaxe AT&T. Multiboot 1, x86 protegido de 32 bits. */
.set MAGIC, 0x1BADB002
.set FLAGS, 3
.section .multiboot,"a"
.balign 4
.long MAGIC
.long FLAGS
.long -(MAGIC + FLAGS)

.section .rodata
.balign 8
gdt:
.quad 0
.quad 0x00CF9A000000FFFF /* codigo, base 0, limite 4 GiB */
.quad 0x00CF92000000FFFF /* dados, base 0, limite 4 GiB */
gdt_end:
gdt_ptr:
.word gdt_end - gdt - 1
.long gdt

.section .bss
.balign 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli
    cld
    mov %eax, %esi
    mov %ebx, %edi
    lgdt gdt_ptr
    ljmp $0x08, $flat_segments
flat_segments:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov $stack_top, %esp
    xor %ebp, %ebp
    sub $8, %esp
    push %edi /* segundo argumento: ponteiro Multiboot */
    push %esi /* primeiro argumento: assinatura */
    call kernel_main
1:  cli
    hlt
    jmp 1b
.size _start, . - _start
.section .note.GNU-stack,"",@progbits

```

## src/kernel.c

```c
/* MiniOS didatico. Sem libc, sem Linux, sem chamadas de sistema. */
#include <stdint.h>
#include <stddef.h>

/* Rotinas exigiveis por um compilador C em ambiente freestanding. */
void *memset(void *dst, int c, size_t n) {
    unsigned char *d = dst;
    for (size_t i = 0; i < n; ++i) d[i] = (unsigned char)c;
    return dst;
}
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if ((uintptr_t)d < (uintptr_t)s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else if (d != s) {
        while (n) { --n; d[n] = s[n]; }
    }
    return dst;
}
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; ++i)
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    return 0;
}
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* COM1 permite usar o terminal do computador hospedeiro e registrar testes. */
#define COM1 0x3F8
static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03); /* 38400 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 bits, sem paridade, 1 stop */
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_put(char c) {
    for (unsigned i = 0; i < 100000; ++i) {
        if (inb(COM1 + 5) & 0x20) {
            outb(COM1, (uint8_t)c);
            return;
        }
    }
}

/* Console VGA texto 80x25. O cursor fisico fica oculto. */
#define WIDTH 80
#define HEIGHT 25
static volatile uint16_t *const vga = (volatile uint16_t *)0xB8000;
static unsigned row, col;
static uint8_t color = 0x0F;
static uint16_t cell(char c) { return ((uint16_t)color << 8) | (uint8_t)c; }
static void clear_screen(void) {
    for (unsigned i = 0; i < WIDTH * HEIGHT; ++i) vga[i] = cell(' ');
    row = col = 0;
}
static void scroll_screen(void) {
    if (row < HEIGHT) return;
    for (unsigned y = 1; y < HEIGHT; ++y)
        for (unsigned x = 0; x < WIDTH; ++x)
            vga[(y - 1) * WIDTH + x] = vga[y * WIDTH + x];
    for (unsigned x = 0; x < WIDTH; ++x) vga[(HEIGHT - 1) * WIDTH + x] = cell(' ');
    row = HEIGHT - 1;
}
static void putchar_os(char c) {
    if (c == '\n') {
        serial_put('\r'); serial_put('\n');
        col = 0; ++row;
    } else if (c == '\b') {
        serial_put('\b'); serial_put(' '); serial_put('\b');
        if (col) --col;
        else if (row) { --row; col = WIDTH - 1; }
        vga[row * WIDTH + col] = cell(' ');
    } else {
        serial_put(c);
        vga[row * WIDTH + col] = cell(c);
        if (++col == WIDTH) { col = 0; ++row; }
    }
    scroll_screen();
}
static void print(const char *s) { while (*s) putchar_os(*s++); }
static void print_u32(uint32_t n) {
    char b[10]; unsigned i = 0;
    do { b[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (i) putchar_os(b[--i]);
}
static int equal(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static int prefix(const char *s, const char *p) {
    while (*p) if (*s++ != *p++) return 0;
    return 1;
}

/* PS/2, conjunto 1, mapa US. Consulta continua, interrupcoes desabilitadas.
   Teclas estendidas, Alt, Ctrl, acentos e LEDs nao sao implementados. */
static const char normal[128] = {
    [2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',
    [10]='9',[11]='0',[12]='-',[13]='=',[14]='\b',
    [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',
    [23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]='\n',
    [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',
    [37]='k',[38]='l',[39]=';',[40]='\'',[41]='`',[43]='\\',
    [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',
    [51]=',',[52]='.',[53]='/',[57]=' '
};
static const char shifted[128] = {
    [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',
    [10]='(',[11]=')',[12]='_',[13]='+',[26]='{',[27]='}',
    [39]=':',[40]='"',[41]='~',[43]='|',[51]='<',[52]='>',[53]='?'
};
static char keyboard_poll(void) {
    static uint8_t shifts, caps, extended;
    uint8_t status = inb(0x64);
    if (!(status & 1)) return 0;
    uint8_t sc = inb(0x60);
    if (status & 0x20) return 0; /* dado do mouse */
    if (sc == 0xE1) { extended = 5; return 0; } /* Pause */
    if (extended) { --extended; return 0; }
    if (sc == 0xE0) { extended = 1; return 0; }
    if (sc == 0x2A) { shifts |= 1; return 0; }
    if (sc == 0x36) { shifts |= 2; return 0; }
    if (sc == 0xAA) { shifts &= (uint8_t)~1; return 0; }
    if (sc == 0xB6) { shifts &= (uint8_t)~2; return 0; }
    if (sc & 0x80) return 0;
    if (sc == 0x3A) { caps ^= 1; return 0; }
    char c = normal[sc];
    if (c >= 'a' && c <= 'z') {
        if ((shifts != 0) ^ (caps != 0)) c -= 'a' - 'A';
    } else if (shifts && shifted[sc]) c = shifted[sc];
    return c;
}
static char input_poll(void) {
    if (inb(COM1 + 5) & 1) {
        char c = (char)inb(COM1);
        return c == '\r' ? '\n' : c;
    }
    return keyboard_poll();
}

/* Apenas o prefixo utilizado da estrutura Multiboot 1. */
struct multiboot_prefix {
    uint32_t flags, mem_lower, mem_upper;
};
static void execute(char *line, uint32_t magic, const struct multiboot_prefix *mb) {
    if (equal(line, "help")) {
        print("help | info | clear | echo TEXTO | color N | halt\n");
        print("color N: 0 a 9, cor do texto; comandos em minusculas.\n");
    } else if (equal(line, "info")) {
        print("MiniOS 1.0 - kernel didatico x86 de 32 bits\n");
        print("Entrada PS/2 ou serial por polling; sem processos e sem disco.\n");
        if (magic == 0x2BADB002 && mb && (mb->flags & 1)) {
            print("Memoria convencional (KiB): "); print_u32(mb->mem_lower);
            print("\nMemoria contigua acima de 1 MiB (KiB): "); print_u32(mb->mem_upper);
            print("\nEstes valores nao representam memoria livre.\n");
        } else print("Informacao de memoria indisponivel.\n");
    } else if (equal(line, "clear")) {
        clear_screen();
        const char *ansi = "\x1b[2J\x1b[H";
        while (*ansi) serial_put(*ansi++);
    } else if (equal(line, "echo")) print("\n");
    else if (prefix(line, "echo ")) { print(line + 5); print("\n"); }
    else if (prefix(line, "color ") && line[6] >= '0' && line[6] <= '9' && !line[7]) {
        color = (uint8_t)(line[6] - '0');
        print("Cor alterada.\n");
    } else if (equal(line, "halt")) {
        print("CPU parada. Feche a janela do QEMU para encerrar.\n");
        for (;;) __asm__ volatile ("cli; hlt");
    } else if (*line) print("Comando desconhecido. Digite help.\n");
}
void kernel_main(uint32_t magic, const struct multiboot_prefix *mb) {
    serial_init();
    outb(0x3D4, 0x0A); outb(0x3D5, 0x20);
    clear_screen();
    print("MiniOS didatico\nKernel proprio em C e Assembly. Digite help.\n");
    if (magic != 0x2BADB002) {
        print("Erro: carregador incompativel com Multiboot 1.\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    char line[128]; size_t used = 0;
    print("minios> ");
    for (;;) {
        char c = input_poll();
        if (!c) { __asm__ volatile ("pause"); continue; }
        if (c == '\n') {
            putchar_os('\n'); line[used] = 0;
            execute(line, magic, mb); used = 0; print("minios> ");
        } else if (c == '\b' || c == 127) {
            if (used) { --used; putchar_os('\b'); }
        } else if (c >= 32 && c <= 126 && used < sizeof(line) - 1) {
            line[used++] = c; putchar_os(c);
        }
    }
}

```

## linker.ld

```ld
ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)
PHDRS {
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 1M;
    .multiboot : { KEEP(*(.multiboot)) } :text
    .text ALIGN(4K) : { *(.text*) } :text
    .rodata ALIGN(4K) : { *(.rodata*) } :text
    .data ALIGN(4K) : { *(.data*) } :data
    .bss ALIGN(4K) : { *(COMMON) *(.bss*) } :data
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}

```

## boot/grub/grub.cfg

```text
set timeout=0
set default=0
terminal_output console
menuentry "MiniOS didatico" {
    multiboot /boot/kernel.elf
    boot
}

```

## Makefile

```makefile
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

```

## tools/check_elf.py

```python
#!/usr/bin/env python3
"""Verifica o contrato de carregamento; nao substitui o teste de boot."""
import struct
import subprocess
import sys
from pathlib import Path
p = Path(sys.argv[1])
b = p.read_bytes()
assert b[:4] == b'\x7fELF' and b[4:6] == b'\x01\x01', 'Esperado ELF32 little endian'
assert struct.unpack_from('<H', b, 18)[0] == 3, 'Esperado EM_386'
found = None
for off in range(0, min(len(b), 8192) - 11, 4):
    magic, flags, checksum = struct.unpack_from('<III', b, off)
    if magic == 0x1BADB002 and (magic + flags + checksum) & 0xFFFFFFFF == 0:
        found = off
        break
assert found is not None, 'Cabecalho Multiboot ausente ou invalido'
undefined = subprocess.check_output(['nm', '-u', str(p)], text=True).strip()
assert not undefined, f'Simbolos indefinidos: {undefined}'
print(f'OK: ELF32 i386, Multiboot no offset {found}, sem simbolos indefinidos.')

```

## tools/emulator_test.py

```python
#!/usr/bin/env python3
"""Teste opcional em Unicorn: pip install unicorn. Nao substitui QEMU/GRUB.
O hardware de E/S e simulado por callbacks, nao por um chipset completo.
"""
from collections import deque
from pathlib import Path
import struct
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INSN, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_INS_IN, UC_X86_INS_OUT,
)

def run(path, serial_input=b'', keyboard=()):
    image = Path(path).read_bytes()
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(0, 16 * 1024 * 1024)
    entry, phoff = struct.unpack_from('<II', image, 24)
    phentsize, phnum = struct.unpack_from('<HH', image, 42)
    for i in range(phnum):
        typ, off, va, _, filesz, _, _, _ = struct.unpack_from('<8I', image, phoff + i * phentsize)
        if typ == 1 and filesz:
            uc.mem_write(va, image[off:off + filesz])
    uc.mem_write(0x8000, struct.pack('<III', 1, 640, 64512))
    uc.reg_write(UC_X86_REG_EAX, 0x2BADB002)
    uc.reg_write(UC_X86_REG_EBX, 0x8000)
    serial = deque(serial_input)
    keys = deque(keyboard)
    output = bytearray()
    state = {'ready': False, 'halted': False}
    def inp(cpu, port, size, data):
        if port == 0x3FD:
            return 0x20 | int(state['ready'] and bool(serial))
        if port == 0x3F8:
            return serial.popleft() if serial else 0
        if port == 0x64:
            return int(state['ready'] and bool(keys))
        if port == 0x60:
            return keys.popleft() if keys else 0
        return 0
    def out(cpu, port, size, value, data):
        if port == 0x3F8:
            # Ignorar divisor de baud inicial (0x03) configurado antes do texto.
            if value != 3:
                output.append(value & 255)
            if output.endswith(b'minios> '):
                state['ready'] = True
    def instruction(cpu, address, size, data):
        if bytes(cpu.mem_read(address, 1)) == b'\xf4':
            state['halted'] = True
            cpu.emu_stop()
    uc.hook_add(UC_HOOK_INSN, inp, None, 1, 0, UC_X86_INS_IN)
    uc.hook_add(UC_HOOK_INSN, out, None, 1, 0, UC_X86_INS_OUT)
    uc.hook_add(UC_HOOK_CODE, instruction)
    uc.emu_start(entry, 0, count=10000000)
    assert state['halted'], 'CPU nao chegou a halt'
    assert not serial and not keys, 'Entrada nao foi totalmente consumida'
    return output.decode('ascii', errors='replace'), uc

path = sys.argv[1] if len(sys.argv) > 1 else 'build/kernel.elf'
commands = (b'help\ninfo\necho teste\necho ac\x08b\ncolor 2\ncolor 99\n'
            b'invalido\n\n' + b'x' * 150 + b'\n' + b'echo scroll\n' * 30 + b'clear\nhalt\n')
text, cpu = run(path, serial_input=commands)
for expected in ('MiniOS didatico', 'Memoria convencional (KiB): 640',
                 '64512', 'teste\r\n', 'Cor alterada.',
                 'Comando desconhecido.', 'CPU parada.'):
    assert expected in text, expected
assert 'x' * 127 in text and 'x' * 128 not in text, 'Limite de linha incorreto'
assert text.count('Comando desconhecido.') == 3
assert 'c\b \bb' in text, 'Backspace nao exercitado'
# PS/2: echo A, echo B e halt. Shift esquerdo e Caps Lock.
keys = [18, 46, 35, 24, 57, 42, 30, 0xAA, 28,
        18, 46, 35, 24, 57, 58, 48, 58, 28,
        35, 30, 38, 20, 28]
text2, _ = run(path, keyboard=keys)
assert 'echo A\r\nA\r\n' in text2
assert 'echo B\r\nB\r\n' in text2
assert 'CPU parada.' in text2
print('OK: entrada Assembly, GDT, pilha, C, VGA e serial em CPU x86 emulada.')
print('OK: comandos, memoria, linha vazia, erros, buffer, Backspace e rolagem.')
print('OK: entrada PS/2 simulada, Shift e Caps Lock.')
print('Pendente: firmware, GRUB, ISO e dispositivos reais em QEMU.')

```
