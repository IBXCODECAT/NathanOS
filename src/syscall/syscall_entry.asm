bits 64

global syscall_entry
extern syscall_dispatch

; SYSCALL entry point — loaded into LSTAR MSR.
; On entry the CPU has already:
;   - saved user RIP  → RCX
;   - saved user RFLAGS → R11 (with SFMASK bits cleared)
;   - set CS/SS to kernel selectors (from STAR)
;   - NOT switched the stack (RSP is still the user stack)
;
; cpu_local layout (pointed to by GS after swapgs):
;   [gs:0]  user_rsp   — scratch for saving user RSP
;   [gs:8]  kernel_rsp — pre-set kernel stack pointer
;
; C target: uint64_t syscall_dispatch(number, arg1, arg2, arg3)
; Syscall:  RAX=number  RDI=arg1  RSI=arg2  RDX=arg3
; SysV:     RDI=number  RSI=arg1  RDX=arg2  RCX=arg3

syscall_entry:
    swapgs                      ; GS ← KERNELGSBASE (= &cpu_local)
    mov  [gs:0], rsp            ; stash user RSP
    mov  rsp,    [gs:8]         ; switch to kernel stack

    push rcx                    ; save user RIP   (needed for sysretq)
    push r11                    ; save user RFLAGS (needed for sysretq)

    ; Remap syscall args → SysV C args
    mov  rcx, rdx               ; arg3 (rdx) → 4th C arg (rcx)
    mov  rdx, rsi               ; arg2 (rsi) → 3rd C arg (rdx)
    mov  rsi, rdi               ; arg1 (rdi) → 2nd C arg (rsi)
    mov  rdi, rax               ; number     → 1st C arg (rdi)
    call syscall_dispatch        ; return value in RAX

    pop  r11                    ; restore user RFLAGS
    pop  rcx                    ; restore user RIP
    mov  rsp, [gs:0]            ; restore user RSP
    swapgs                      ; GS ← user GSBASE, KERNELGSBASE ← &cpu_local
    o64 sysret                  ; 64-bit SYSRET: return to ring 3 (RCX→RIP, R11→RFLAGS)
