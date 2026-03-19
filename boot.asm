BITS 32
section .mboot
    align 4
    dd 0x1BADB002              ; Magic
    dd 0x00000000              ; Flags
    dd -(0x1BADB002 + 0x00000000) ; Checksum

section .text
global start
extern kmain

start:
    cli
    mov esp, stack_space + 8192 ; Point to the TOP of the stack
    call kmain
    hlt

section .bss
resb 8192
stack_space:
