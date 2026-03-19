bits 64

; Flat binary — no org; loaded at USER_LOAD_ADDR by the kernel.
; All data references use RIP-relative addressing so the binary
; runs correctly at whatever virtual address the kernel places it.

_start:
    mov  rax, 1             ; SYS_WRITE
    mov  rdi, 1             ; fd (ignored by our sys_write — always → VGA)
    lea  rsi, [rel msg]     ; buf
    mov  rdx, msg_len       ; len
    syscall

    mov  rax, 60            ; SYS_EXIT
    xor  rdi, rdi           ; code = 0
    syscall

msg:
    db "Hello I am a user mode binary", 10
msg_len equ $ - msg
