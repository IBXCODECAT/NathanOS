# Drivers

**Sources:** `src/drivers/vga.c`, `src/drivers/keyboard.c`, `src/drivers/timer.c`

---

## VGA Text Mode

**Source:** `src/drivers/vga.c`

### Hardware

The VGA text buffer is memory-mapped at physical address `0xB8000`, inside the identity-mapped first 64 MB. The buffer is 80 columns × 25 rows = 2,000 cells; each cell is a 16-bit value:

```
Bits 15–12: background color (4 bits)
Bits 11–8:  foreground color (4 bits)
Bits 7–0:   ASCII character
```

```c
static uint16_t* const VGA_BUFF = (uint16_t*)0xB8000;

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}
```

### Colors

Standard 4-bit VGA palette (values 0–15). `vga_set_color(fg, bg)` packs them into `term_color`:

```c
term_color = (bg << 4) | (fg & 0x0F);
```

The default is white on black. The panic handler sets white on red (`VGA_COLOR_WHITE`, `VGA_COLOR_RED`) before printing.

### Hardware Cursor

The VGA cursor position is updated via I/O ports `0x3D4` (index) and `0x3D5` (data) after every character write. The linear position is `row * 80 + col`:

```c
outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);        // low byte
outb(0x3D4, 0x0E); outb(0x3D5, (pos >> 8) & 0xFF);  // high byte
```

### Scrolling

When `term_row` reaches 25 (past the last row), `vga_putc` shifts every row up by one (copies row N to row N−1), clears the bottom row, and resets `term_row` to 24. This happens entirely in software by copying within `VGA_BUFF`.

---

## Keyboard

**Source:** `src/drivers/keyboard.c`

### Hardware

The PS/2 keyboard controller signals keypress and key-release events via IRQ 1. The driver reads the **scancode** from I/O port `0x60` immediately in the IRQ handler.

```c
static void keyboard_handler(registers_t* regs) {
    uint8_t scancode = inb(KB_DATA_PORT);  // 0x60
    ...
}
void keyboard_init(void) {
    irq_register_handler(1, keyboard_handler);
    ...
}
```

### Scancode Decoding

nOS uses PS/2 Scancode Set 1. Make codes (key down) are in the range 0x01–0x58; break codes (key up) are the make code with bit 7 set (`| 0x80`). Two lookup tables map make codes to ASCII:

```c
static const char scancode_normal[128]  = { ... };  // unshifted
static const char scancode_shifted[128] = { ... };  // with Shift held
```

Shift state is tracked with a boolean flag updated on `SC_LSHIFT` / `SC_RSHIFT` make/break events. Any scancode with bit 7 set that isn't a shift-release is silently ignored (it's a key-release).

### Esc-Hold Soft Panic

Holding Escape for **10 seconds** triggers `panic(PANIC_MANUALLY_INITIATED_PANIC)`. This is implemented as a timer callback:

```c
static void keyboard_on_tick(uint64_t ticks) {
    if (esc_held && ticks - esc_start_tick >= ESC_HOLD_TICKS)  // 10 * TIMER_HZ
        panic(PANIC_MANUALLY_INITIATED_PANIC);
}
```

Auto-repeat (the keyboard hardware re-sends the make code while a key is held) is handled by only recording `esc_start_tick` on the *first* Esc make event (`if (!esc_held)`).

---

## Timer (PIT)

**Source:** `src/drivers/timer.c`

### Hardware

The Intel 8253/8254 Programmable Interval Timer has three independent channels. nOS uses **channel 0**, which is wired to IRQ 0. The PIT oscillator runs at 1,193,182 Hz. A 16-bit divisor sets the output frequency:

```
f_out = 1,193,182 / divisor
```

`TIMER_HZ` is 100 (defined in `timer.h`), giving a divisor of 11,931 and a tick interval of **~10 ms**.

### Initialization

```c
#define PIT_DIVISOR  (1193182 / TIMER_HZ)   // = 11931

void timer_init(void) {
    outb(PIT_CMD,      0x36);                         // channel 0, lo/hi, mode 3
    outb(PIT_CHANNEL0, PIT_DIVISOR & 0xFF);           // low byte
    outb(PIT_CHANNEL0, (PIT_DIVISOR >> 8) & 0xFF);   // high byte
    irq_register_handler(0, timer_handler);
}
```

Command byte `0x36`: select channel 0, access mode lo/hi (send low byte then high byte), mode 3 (square wave generator), binary (not BCD).

### Tick Counter and Callback

Every IRQ 0 increments a `volatile uint64_t ticks` counter and calls the registered callback:

```c
static void timer_handler(registers_t* regs) {
    ticks++;
    if (tick_callback) tick_callback(ticks);
}
```

Only **one** callback slot exists — calling `timer_register_tick_callback` again replaces the previous one. In `kmain`, `keyboard_init()` runs before `sched_init()`, so `keyboard_on_tick` is registered first and then overwritten by `schedule_tick`. As a result the Esc-hold panic does not fire once the scheduler is active — a known limitation of the single-slot design. Fixing it requires either a multi-callback list or chaining the two callbacks manually.

`timer_get_ticks()` returns the current tick count and is used by the keyboard driver to measure Esc hold duration.
