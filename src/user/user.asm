bits 64

section .text
global _start

_start:
    mov  rax, 1             ; SYS_WRITE
    mov  rdi, 1             ; fd (→ VGA)
    lea  rsi, [rel msg]     ; buf
    mov  rdx, msg_len       ; len
    syscall

    mov  rax, 60            ; SYS_EXIT
    xor  rdi, rdi           ; code = 0
    syscall

section .rodata
msg:        db "Hello I am a user mode binary", 10
msg_len equ $ - msg
