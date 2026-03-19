bits 64
section .text

; ISR stub without error code: CPU didn't push one, so we push a dummy 0
%macro ISR_NOERR 1
global isr%1
isr%1:
    push 0          ; dummy error code
    push %1         ; interrupt number
    jmp isr_common
%endmacro

; ISR stub with error code: CPU already pushed the error code
%macro ISR_ERR 1
global isr%1
isr%1:
    push %1         ; interrupt number (error code is already on the stack below)
    jmp isr_common
%endmacro

; CPU Exceptions 0-31
; Exceptions with an error code: 8, 10, 11, 12, 13, 14, 17, 21, 30
ISR_NOERR  0   ; #DE  Division by Zero
ISR_NOERR  1   ; #DB  Debug
ISR_NOERR  2   ;      Non-Maskable Interrupt
ISR_NOERR  3   ; #BP  Breakpoint
ISR_NOERR  4   ; #OF  Overflow
ISR_NOERR  5   ; #BR  Bound Range Exceeded
ISR_NOERR  6   ; #UD  Invalid Opcode
ISR_NOERR  7   ; #NM  Device Not Available
ISR_ERR    8   ; #DF  Double Fault (error code always 0)
ISR_NOERR  9   ;      Coprocessor Segment Overrun (legacy)
ISR_ERR   10   ; #TS  Invalid TSS
ISR_ERR   11   ; #NP  Segment Not Present
ISR_ERR   12   ; #SS  Stack-Segment Fault
ISR_ERR   13   ; #GP  General Protection Fault
ISR_ERR   14   ; #PF  Page Fault
ISR_NOERR 15   ;      Reserved
ISR_NOERR 16   ; #MF  x87 FPU Error
ISR_ERR   17   ; #AC  Alignment Check
ISR_NOERR 18   ; #MC  Machine Check
ISR_NOERR 19   ; #XM  SIMD Floating-Point Exception
ISR_NOERR 20   ; #VE  Virtualization Exception
ISR_ERR   21   ; #CP  Control Protection Exception
ISR_NOERR 22   ;      Reserved
ISR_NOERR 23   ;      Reserved
ISR_NOERR 24   ;      Reserved
ISR_NOERR 25   ;      Reserved
ISR_NOERR 26   ;      Reserved
ISR_NOERR 27   ;      Reserved
ISR_NOERR 28   ; #HV  Hypervisor Injection Exception
ISR_NOERR 29   ; #VC  VMM Communication Exception
ISR_ERR   30   ; #SX  Security Exception
ISR_NOERR 31   ;      Reserved

; Common entry point for all ISRs.
;
; At this point the stack looks like (high to low):
;   SS, old RSP, RFLAGS, CS, RIP  <- pushed by CPU
;   error_code (real or dummy 0)   <- pushed by CPU or stub
;   int_num                        <- pushed by stub
;
; We save all general-purpose registers, call the C handler with
; a pointer to the saved state, then restore everything and iretq.
extern isr_handler

isr_common:
    ; Save all GPRs (pushed high -> low, so struct reads low -> high as r15..rax)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; First argument (RDI) = pointer to the saved registers struct
    mov rdi, rsp
    call isr_handler

    ; Restore GPRs
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Discard int_num and error_code, then return from interrupt
    add rsp, 16
    iretq
