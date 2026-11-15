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
