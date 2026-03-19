# Interrupts

**Sources:** `src/idt/idt.h`, `src/idt/idt.c`, `src/idt/isr.asm`

## What the IDT Is

When the CPU encounters an exception (divide-by-zero, page fault, etc.) or a hardware interrupt (timer tick, keypress), it looks up a handler address in the **Interrupt Descriptor Table (IDT)**. The IDT is an array of up to 256 gate descriptors, each 16 bytes, indexed by interrupt vector number. The CPU register `IDTR` holds the base address and limit of the table; `lidt` loads it.

NathanOS installs 48 entries (vectors 0–47):
- 0–31: CPU exceptions
- 32–47: hardware IRQs (after PIC remapping — see below)

## Gate Descriptor Layout

Each IDT entry (`idt_entry_t` in `idt.h`) is 16 bytes:

```
 63            48 47 46-45 44 43-40 39-32 31          16 15           0
┌───────────────┬──┬─────┬──┬──────┬──────┬─────────────┬──────────────┐
│  offset[63:32]│  │ DPL │  │ type │ IST  │ offset[31:16]│ offset[15:0] │
└───────────────┴──┴─────┴──┴──────┴──────┴─────────────┴──────────────┘
                 P        S         res
```

Fields in the C struct:

| Field | Value | Meaning |
|-------|-------|---------|
| `offset_low` | handler VA bits 0–15 | |
| `selector` | `0x08` | Kernel code segment (GDT offset 8) |
| `ist` | `0` | No interrupt stack table switching |
| `type_attr` | `0x8E` | Present \| DPL=0 \| 64-bit interrupt gate (type 0xE) |
| `offset_mid` | handler VA bits 16–31 | |
| `offset_high` | handler VA bits 32–63 | |
| `reserved` | `0` | |

Type `0xE` is a 64-bit interrupt gate. Unlike a trap gate, an interrupt gate clears `IF` (the interrupt flag) on entry, so interrupt handlers are not themselves interrupted.

## PIC Remapping

The 8259A Programmable Interrupt Controller (PIC) has a master and a slave chip, each handling 8 IRQ lines. The BIOS configures them to raise vectors 8–15 (master) and 0x70–0x77 (slave). **These collide with CPU exceptions in protected/long mode** (e.g. IRQ 0 would fire vector 8, which is the double fault exception handler).

`pic_remap()` in `idt.c` reinitializes both chips to use vectors 32–39 (master) and 40–47 (slave):

```c
outb(PIC1_DATA, 32);   // master: IRQ 0-7  → INT 32-39
outb(PIC2_DATA, 40);   // slave:  IRQ 8-15 → INT 40-47
```

After remapping, all IRQs are masked except IRQ 0 (timer) and IRQ 1 (keyboard):

```c
outb(PIC1_DATA, 0xFC);  // 1111 1100 — unmask bit 0 (timer) and bit 1 (keyboard)
outb(PIC2_DATA, 0xFF);  // all slave IRQs masked
```

## ISR Stub Pattern

Handling 48 vectors uniformly requires a small shim per vector. Two NASM macros in `isr.asm` generate these stubs:

```nasm
; Exceptions WITHOUT an error code: push dummy 0 first
%macro ISR_NOERR 1
isr%1:
    push 0      ; dummy error code — keeps stack layout uniform
    push %1     ; interrupt number
    jmp isr_common
%endmacro

; Exceptions WITH an error code: CPU already pushed it
%macro ISR_ERR 1
isr%1:
    push %1     ; interrupt number (error code is already below on the stack)
    jmp isr_common
%endmacro
```

The distinction exists because some exceptions (8 #DF, 10 #TS, 11 #NP, 12 #SS, 13 #GP, 14 #PF, 17 #AC, 21 #CP, 30 #SX) push a CPU-supplied error code; all others do not. By pushing a dummy 0 for the no-error variants, every vector arrives at `isr_common` with an identical stack shape.

## `isr_common` and the `registers_t` Stack Frame

All 48 stubs jump to `isr_common`, which:

1. Pushes all 15 general-purpose registers (rax through r15)
2. Passes RSP (pointing at the complete saved frame) as the first argument
3. Calls `isr_handler` in C
4. Pops the GPRs
5. Discards `int_num` and `error_code` with `add rsp, 16`
6. Returns with `iretq` (which atomically restores RIP, CS, RFLAGS, RSP, SS)

The stack layout (from high to low address, i.e. in push order) when `isr_handler` is called:

```
High addresses (pushed first by CPU)
  SS
  RSP (old)
  RFLAGS
  CS
  RIP
  error_code   ← CPU-pushed or dummy 0
  int_num      ← stub pushes this
  rax          ← isr_common starts here
  rbx
  rcx
  rdx
  rsi
  rdi
  rbp
  r8
  r9
  r10
  r11
  r12
  r13
  r14
  r15          ← RSP points here when isr_handler is called
Low addresses
```

This exactly mirrors the `registers_t` struct in `idt.h`:

```c
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_num, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;
```

## IRQ Dispatch and EOI-Before-Handler

`isr_handler` in `idt.c` dispatches based on `int_num`:

- **Vectors 0–31** (exceptions): look up the corresponding `panic_code_t` in `exception_codes[]` and call `panic_ex`. Execution never returns.
- **Vectors 32–47** (IRQs): compute `irq = int_num - 32`, send EOI, call the registered handler.

**Why EOI before the handler?** If the scheduler runs during an IRQ handler (which it does — the timer tick calls `schedule()`), `iretq` will resume the *new* task rather than the interrupted one. At that point we are permanently "inside" the IRQ handler from the PIC's perspective. If EOI hasn't been sent, the PIC will never unmask that IRQ line and the system stalls after the very first context switch. Sending EOI first, while `IF=0` (interrupt gate keeps interrupts off), is safe because no new IRQ can arrive during the window between EOI and the end of the handler.

```c
if (irq >= 8) outb(PIC2_CMD, 0x20);  // EOI to slave
outb(PIC1_CMD, 0x20);                 // EOI to master
if (irq_handlers[irq]) irq_handlers[irq](regs);
```

Drivers register their handlers with `irq_register_handler(irq, fn)`. The table supports IRQs 0–15; registering an out-of-range number panics.
