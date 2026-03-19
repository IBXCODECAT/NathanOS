# GDT & TSS

**Sources:** `src/gdt/gdt.h`, `src/gdt/gdt.c`, `src/gdt/gdt_flush.asm`

The boot-time GDT (`boot.asm`) contains only a null descriptor and a 64-bit kernel code segment — enough to enter long mode, but not enough for privilege separation or syscalls. `gdt_init()` replaces it at runtime with a full 7-descriptor table that includes user-mode segments and a Task State Segment (TSS).

---

## GDT Layout

| # | Selector | Used as | Description |
|---|----------|---------|-------------|
| 0 | `0x00` | — | Null (required by the architecture) |
| 1 | `0x08` | `0x08` | Kernel code — DPL=0, L=1 (64-bit), access=`0x9A` |
| 2 | `0x10` | `0x10` | Kernel data — DPL=0, access=`0x92` |
| 3 | `0x18` | `0x1B` | **User data** — DPL=3, access=`0xF2`; `\|RPL=3` → `0x1B` |
| 4 | `0x20` | `0x23` | **User code** — DPL=3, L=1, access=`0xFA`; `\|RPL=3` → `0x23` |
| 5 | `0x28` | `0x28` | TSS (low 8 bytes of 16-byte system descriptor) |
| 6 | `0x30` | — | TSS (high 8 bytes — upper 32 bits of base address) |

**User data must come before user code** (entries 3 and 4). The `SYSRET` instruction hardcodes its segment selector calculation relative to `STAR[63:48]`: it loads SS as `STAR[63:48] + 8` and CS as `STAR[63:48] + 16`. With `STAR[63:48] = 0x10`, that gives `0x18 | 3` for SS and `0x20 | 3` for CS — matching the user data/code positions.

---

## Task State Segment (TSS)

The TSS is the mechanism the CPU uses to find the kernel stack when an interrupt or exception fires while the CPU is in ring 3. Without a TSS, a ring-3 interrupt would push the exception frame onto the user stack — which the kernel must not trust.

```c
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;       // kernel stack pointer for ring 3 → ring 0 transitions
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];     // interrupt stack table (not used yet)
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;       // I/O permission bitmap offset
} __attribute__((packed)) tss_t;
```

`tss.rsp0` is what the CPU reads on every interrupt from ring 3. `gdt_set_kernel_stack(rsp0)` updates it; it must be called on each task switch to point at the new task's kernel stack.

`tss.iopb` is set to `sizeof(tss_t)` so the I/O permission bitmap is "outside" the TSS segment. This denies all port I/O to ring-3 code (any `in`/`out` from ring 3 triggers a #GP).

---

## Descriptor Encoding

An 8-byte GDT code/data descriptor:

```
 63      56 55    52 51     48 47      40 39     16 15       0
┌──────────┬────────┬─────────┬──────────┬─────────┬──────────┐
│ base[31:24]│  gran  │ limit[19:16]│  access  │ base[23:0]│ limit[15:0]│
└──────────┴────────┴─────────┴──────────┴─────────┴──────────┘
```

Key access byte bits for 64-bit segments:
- `0x9A` = `1001 1010` — Present, DPL=0, non-system, executable, readable
- `0xFA` = `1111 1010` — Present, DPL=3, non-system, executable, readable
- `0x92` = `1001 0010` — Present, DPL=0, non-system, writable data
- `0xF2` = `1111 0010` — Present, DPL=3, non-system, writable data

Key granularity byte bits:
- `0x20` — L=1 (64-bit code segment); all limit fields ignored in 64-bit mode

The TSS descriptor is 16 bytes (two consecutive GDT slots) to accommodate the full 64-bit base address:

```c
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;      // 0x89 = Present, system, TSS available
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;  // upper 32 bits of base (second slot)
    uint32_t reserved;
} __attribute__((packed)) tss_desc_t;
```

---

## `gdt_init`

1. Programs all descriptor fields in a static `gdt_table_t`.
2. Fills the TSS descriptor with `base = &tss`, `limit = sizeof(tss_t) - 1`.
3. Calls `gdt_flush(&gdt_ptr, GDT_KERNEL_CODE, GDT_KERNEL_DATA)` to `lgdt` and reload CS/SS/DS/ES/FS/GS.
4. Calls `tss_load(GDT_TSS_SEL)` to `ltr` the TSS selector.

---

## `gdt_flush.asm` — Reloading CS

In 64-bit mode a far `jmp` or far `call` cannot be encoded with an immediate selector — the selector must come from memory or a register. The standard trick is a **far return**: push CS then RIP onto the stack and execute `retf`, which pops RIP first, then CS, atomically reloading both.

```nasm
gdt_flush:                   ; rdi=gdt_ptr, rsi=cs, rdx=ds
    lgdt [rdi]
    push rsi                 ; CS at higher address
    lea  rax, [rel .reload]
    push rax                 ; RIP at lower address (top of stack)
    o64 retf                 ; pops RIP then CS — 64-bit far return
.reload:
    mov ds, dx
    mov es, dx
    mov ss, dx
    xor eax, eax
    mov fs, ax
    mov gs, ax
    ret
```

`o64 retf` (not bare `retf`) is required in NASM's `bits 64` mode. Without the `o64` prefix, NASM encodes a 32-bit far return, which pops only 4 bytes for RIP and 4 bytes for CS. The upper 32 bits of the `.reload` address (all zeros for addresses below 4 GB) would be loaded as CS, selecting the null descriptor and triggering a General Protection Fault.

---

## `ring3_enter`

```c
__attribute__((noreturn)) void ring3_enter(uint64_t entry, uint64_t user_rsp);
```

Builds a 5-word `iretq` frame on the kernel stack and executes `iretq` to drop into ring 3:

```
High address
  [+32] SS  = GDT_USER_DATA | 3  (0x1B)
  [+24] RSP = user_rsp
  [+16] RFLAGS (with IF=1 so interrupts work in user mode)
  [+8]  CS  = GDT_USER_CODE | 3  (0x23)
  [+0]  RIP = entry              ← RSP points here
Low address
```

`iretq` pops RIP → CS → RFLAGS → RSP → SS in that order and switches the CPU to ring 3, loading the user stack pointer.
