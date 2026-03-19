# Boot

**Source:** `src/boot.asm`

## Why a Bootloader Shim?

GRUB loads the kernel and hands control to it, but it does so in **32-bit protected mode** — not the 64-bit long mode that the rest of the kernel expects. `boot.asm` is the bridge: it runs in 32-bit, builds the minimal page tables and GDT needed for long mode, and then jumps to `kmain` in 64-bit.

## Step-by-Step Walkthrough

### 1. Multiboot Header

```nasm
section .mboot
    align 4
    dd 0x1BADB002              ; Magic number GRUB looks for
    dd 0x00000002              ; Flags: bit 1 asks GRUB to populate mem_lower/mem_upper
    dd -(0x1BADB002 + 0x00000002) ; Checksum: magic + flags + checksum must equal 0
```

GRUB scans the first 8 KB of the kernel binary for this 12-byte header. If found, it loads the kernel and jumps to the `start` symbol with `EBX` pointing at the Multiboot Information (MBI) struct.

### 2. Temporary 32-bit Stack

```nasm
mov esp, stack_top
```

An 8 KB stack is reserved in `.bss` (`stack_bottom`/`stack_top`). This stack is only used during the 32-bit setup phase; once in long mode the same physical memory backs the initial kernel stack.

### 3. Building the Page Tables

x86-64 requires a 4-level page table hierarchy: **PML4 → PDPT → PD → PT**. For the initial identity map, the boot code only needs three levels because it uses 2 MB huge pages (so no PT level).

All three tables live in `.bss` (zero-initialized, 4 KB each):

```nasm
section .bss
align 4096
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096
```

Linking them together:

```nasm
; PML4[0] → PDPT (present + writable)
mov eax, pdpt_table
or eax, 0b11
mov [pml4_table], eax

; PDPT[0] → PD (present + writable)
mov eax, pd_table
or eax, 0b11
mov [pdpt_table], eax
```

### 4. 32 × 2 MB Huge-Page Identity Map (First 64 MB)

```nasm
mov ecx, 0
.map_pd_huge:
    mov eax, ecx
    shl eax, 21                ; eax = ecx * 2 MB
    or eax, 0b10000011         ; Present + Writable + PS (huge page)
    mov [pd_table + ecx * 8], eax
    inc ecx
    cmp ecx, 32               ; 32 entries × 2 MB = 64 MB
    jne .map_pd_huge
```

Bit 7 (`PS`) in a PD entry tells the CPU this is a 2 MB huge page rather than a pointer to a PT. This means physical address 0 maps to virtual address 0, address 0x200000 maps to itself, and so on up to 64 MB.

**Why 64 MB?** The PMM places its bitmap immediately after `_kernel_end`. The bitmap is one bit per 4 KB page, so tracking 500 GB of RAM takes about 16 MB of bitmap. 64 MB is comfortably larger than any realistic kernel + bitmap size while still fitting in a single PD (which covers 1 GB maximum). See [memory.md](memory.md) for details.

### 5. Enable PAE (Physical Address Extension)

```nasm
mov eax, cr4
or eax, 1 << 5      ; PAE bit
mov cr4, eax
```

PAE is required before long mode can be activated. It widens physical addresses to 52 bits and enables 4-level paging.

### 6. Load PML4 into CR3

```nasm
mov eax, pml4_table
mov cr3, eax
```

CR3 holds the physical address of the top-level page table (PML4). The CPU uses this as the root for all virtual-to-physical translations.

### 7. Enable Long Mode via EFER MSR

```nasm
mov ecx, 0xC0000080    ; EFER MSR number
rdmsr
or eax, 1 << 8         ; LME (Long Mode Enable) bit
wrmsr
```

The EFER (Extended Feature Enable Register) is a Model Specific Register. Setting bit 8 (LME) tells the CPU to enter long mode as soon as paging is enabled.

### 8. Enable Paging

```nasm
mov eax, cr0
or eax, 1 << 31        ; PG bit
mov cr0, eax
```

Setting bit 31 of CR0 enables paging. Because LME is already set in EFER, the CPU activates 64-bit long mode at this point — but the current code segment still has the 32-bit descriptor from the BIOS/GRUB GDT.

### 9. Load the 64-bit GDT and Far Jump to Flush CS

```nasm
lgdt [gdt64.pointer]
jmp gdt64.code:long_mode_start
```

The far jump atomically reloads CS with the new 64-bit code descriptor. Until CS is reloaded, the CPU is technically in a "compatibility mode" sub-state of long mode. After the jump, execution continues in `long_mode_start` as full 64-bit code.

## GDT Structure

```nasm
section .rodata
gdt64:
    dq 0                                              ; Null descriptor (required)
.code: equ $ - gdt64                                  ; Offset = 8 = kernel CS selector
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)         ; Code descriptor
```

The code descriptor bits:
- Bit 43: Executable (this is a code segment, not data)
- Bit 44: Descriptor type = 1 (code/data, not system)
- Bit 47: Present
- Bit 53: Long mode (L bit) — required for 64-bit code segments

There is no data descriptor because 64-bit mode ignores most segment attributes for DS/ES/FS/GS anyway.

## Entering `kmain`

```nasm
BITS 64
long_mode_start:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov edi, ebx       ; MBI pointer → first argument (RDI, SysV AMD64 ABI)
    call kmain
    hlt
```

Segment registers are zeroed (null selector is fine in 64-bit mode for everything except CS). The Multiboot Information pointer that GRUB left in EBX is moved to EDI, becoming the first argument to `kmain` per the System V AMD64 calling convention.
