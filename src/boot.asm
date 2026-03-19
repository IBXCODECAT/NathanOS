BITS 32

; Multiboot header for GRUB
section .mboot
    align 4
    dd 0x1BADB002              ; Magic number
    dd 0x00000000              ; Flags
    dd -(0x1BADB002 + 0x00000000) ; Checksum

section .text
global start
extern kmain

start:
    mov esp, stack_top         ; Set up temporary 32-bit stack

    ; 1. Link PML4 -> PDPT
    mov eax, pdpt_table
    or eax, 0b11               ; Present + Writable
    mov [pml4_table], eax

    ; 2. Link PDPT -> PD
    mov eax, pd_table
    or eax, 0b11
    mov [pdpt_table], eax

    ; 3. Link PD -> PT
    mov eax, pt_table
    or eax, 0b11
    mov [pd_table], eax

    ; 4. Map PT entries to 4KiB pages (Identity map first 2MB)
    mov ecx, 0
.map_pt:
    mov eax, 0x1000            ; 4KiB size
    mul ecx
    or eax, 0b11               ; Present + Writable
    mov [pt_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512               ; 512 entries = 2MB
    jne .map_pt

    ; 5. Enable PAE (Physical Address Extension)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 6. Load PML4 into CR3 (Tells CPU where page tables start)
    mov eax, pml4_table
    mov cr3, eax

    ; 7. Enable Long Mode in EFER MSR (Model Specific Register)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 8. Enable Paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; 9. Load 64-bit GDT and perform the Far Jump to Long Mode
    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

BITS 64
long_mode_start:
    ; Clean up segment registers for 64-bit mode
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call kmain                 ; Jump to your 64-bit C code!
    hlt

section .rodata
; The 64-bit Global Descriptor Table
gdt64:
    dq 0 ; Null descriptor
.code: equ $ - gdt64
    ; Code descriptor: Access (present, ring 0, code) + Long Mode bit
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) 
.pointer:
    dw $ - gdt64 - 1           ; Size
    dq gdt64                   ; Address

section .bss
align 4096                     ; Page tables MUST be 4KB aligned
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096
pt_table:   resb 4096          ; 4th level table for 4KB pages
stack_bottom: resb 8192
stack_top:
