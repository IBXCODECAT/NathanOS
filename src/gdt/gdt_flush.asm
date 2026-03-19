bits 64

global gdt_flush
global tss_load

; gdt_flush(gdt_ptr_t* rdi, uint16_t rsi=cs, uint16_t rdx=ds)
gdt_flush:
    lgdt [rdi]
    push rsi                    ; CS — higher address (retf pops this second)
    lea  rax, [rel .reload]     ; RIP — lower address (retf pops this first)
    push rax
    o64 retf                    ; 64-bit far return: pops 8-byte RIP then 8-byte CS
.reload:
    mov ds, dx
    mov es, dx
    mov ss, dx
    xor eax, eax
    mov fs, ax
    mov gs, ax
    ret

; tss_load(uint16_t di=selector)
tss_load:
    ltr di
    ret
