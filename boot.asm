MBALIGN equ 1<<0
MEMINFO equ 1<<1
FLAGS equ MBALIGN | MEMINFO
MAGIC equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
dd MAGIC, FLAGS, CHECKSUM

section .bss
align 16
stack_bottom: resb 16384
stack_top:

section .text
global _start
extern kernel_main
_start:
    mov esp, stack_top
    push ebx            ; 2nd argument: multiboot info pointer
    push eax            ; 1st argument: multiboot magic
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang