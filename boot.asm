BITS 32
section .mboot
    align 4
    dd 0x1BADB002
    dd 0x00000000
    dd -(0x1BADB002 + 0x00000000)

section .text
global start
extern kmain

start:
    mov esp, stack_top

    ; 1. Set up Paging (PML4 -> PDPT -> PD)
    mov eax, pdpt_table
    or eax, 0b11 ; present + writable
    mov [pml4_table], eax

    mov eax, pd_table
    or eax, 0b11 ; present + writable
    mov [pdpt_table], eax

    ; 2. Map PD entries to 2MiB huge pages
    mov ecx, 0
.map_pd:
    mov eax, 0x200000 ; 2MiB
    mul ecx
    or eax, 0b10000011 ; present + writable + huge
    mov [pd_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; 3. Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 4. Load PML4 into CR3
    mov eax, pml4_table
    mov cr3, eax

    ; 5. Enable Long Mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 6. Enable Paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; 7. Load 64-bit GDT and Jump
    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

BITS 64
long_mode_start:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    call kmain
    hlt

section .rodata
gdt64:
    dq 0 ; null
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .bss
align 4096
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096
stack_bottom: resb 8192
stack_top:
