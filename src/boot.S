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
