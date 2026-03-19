bits 64

section .text
global _start

_start:
.loop:
    ; Print prompt
    mov  rax, 1
    mov  rdi, 1
    lea  rsi, [rel prompt]
    mov  rdx, prompt_len
    syscall

    ; Read a line (SYS_READ blocks until newline; echoes as you type)
    mov  rax, 0
    mov  rdi, 0
    lea  rsi, [rel line_buf]
    mov  rdx, 128
    syscall
    ; rax = bytes read (includes the newline)
    test rax, rax
    jz   .loop

    ; Write the line back
    mov  rdx, rax
    mov  rax, 1
    mov  rdi, 1
    lea  rsi, [rel line_buf]
    syscall

    jmp  .loop

section .rodata
prompt:     db "> "
prompt_len  equ $ - prompt

section .bss
line_buf:   resb 128
