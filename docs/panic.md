# Panic

**Sources:** `src/panic/panic.h`, `src/panic/panic.c`

## The X-Macro Pattern

Every OS needs a way to halt with a meaningful error message when something goes unrecoverably wrong. NathanOS also needs the same set of error codes to appear in two places: as enum values (for `switch`/comparison) and as strings (for display). Keeping these in sync manually is error-prone.

The solution is an **X-macro list** — a single macro that defines all codes, applied twice with different expansions:

```c
// panic.h
#define PANIC_CODE_LIST \
    X(DIVIDE_BY_ZERO)                   \
    X(GENERAL_PROTECTION_FAULT)         \
    X(PAGE_FAULT_IN_NON_PAGED_AREA)     \
    X(PMM_DOUBLE_FREE)                  \
    X(HEAP_DOUBLE_FREE)                 \
    X(VMM_HUGEPAGE_CONFLICT)            \
    X(MANUALLY_INITIATED_PANIC)         \
    // ... (38 codes total)

// Expansion 1: enum
typedef enum {
#define X(name) PANIC_##name,
    PANIC_CODE_LIST
#undef X
    PANIC_CODE_COUNT
} panic_code_t;

// Expansion 2: string table (in panic.c)
static const char* panic_strings[] = {
#define X(name) #name,
    PANIC_CODE_LIST
#undef X
};
```

Adding a new panic code is a single line in `PANIC_CODE_LIST`. The enum value and the display string are both generated automatically — they can never diverge.

## Panic Code Categories

| Category | Examples |
|----------|---------|
| CPU exceptions | `DIVIDE_BY_ZERO`, `GENERAL_PROTECTION_FAULT`, `PAGE_FAULT_IN_NON_PAGED_AREA`, `DOUBLE_FAULT` |
| IRQ system | `INVALID_IRQ_NUMBER` |
| Boot | `NO_MULTIBOOT_INFO` |
| PMM | `PMM_NULL_MULTIBOOT_INFO`, `PMM_NO_MEMORY_DETECTED`, `PMM_BITMAP_EXCEEDS_MAPPED_REGION`, `PMM_DOUBLE_FREE`, `PMM_FREE_OUT_OF_RANGE` |
| Heap | `OUT_OF_MEMORY`, `HEAP_DOUBLE_FREE` |
| VMM | `VMM_CR3_INVALID`, `VMM_UNALIGNED_ADDRESS`, `VMM_HUGEPAGE_CONFLICT` |
| Manual | `MANUALLY_INITIATED_PANIC` |

## API

```c
void panic(panic_code_t code);
void panic_ex(panic_code_t code, uint64_t rip, uint64_t err, uint64_t fault_addr);
```

`panic(code)` is used by subsystems that detect an internal invariant violation (double-free, out of memory, etc.) where there are no useful register values to display.

`panic_ex(code, rip, err, fault_addr)` is used by the exception dispatcher in `idt.c` — it has the faulting RIP, the CPU error code, and (for page faults) the address in CR2.

## BSOD Display

Both functions call the internal `do_panic()`:

1. Sets terminal color to white-on-red (`vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED)`).
2. Clears the screen (`vga_cls()`).
3. Prints a Windows-style BSOD message with the code name.
4. If called via `panic_ex`, also prints `RIP`, `ERR`, and (if non-zero) `FAULT ADDR` in hex.
5. Disables interrupts (`cli()`) and loops on `hlt()` — the system is dead.

Example output for a page fault:

```
NathaNathanOS has encountered a fatal error and has been shut down to prevent
damage to your computer.

PAGE_FAULT_IN_NON_PAGED_AREA

...

*** PANIC: PAGE_FAULT_IN_NON_PAGED_AREA

RIP: 0xFFFFFFFF80001234  ERR: 0x0000000000000002
FAULT ADDR: 0x0000000000000000
```

## Adding a New Panic Code

Add one line to `PANIC_CODE_LIST` in `panic.h`:

```c
#define PANIC_CODE_LIST \
    ...
    X(MY_NEW_ERROR)       \   // ← add here
    X(MANUALLY_INITIATED_PANIC)
```

That's it. The enum gains `PANIC_MY_NEW_ERROR` and the string table gains `"MY_NEW_ERROR"` with no other changes.
