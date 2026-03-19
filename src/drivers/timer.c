#include "timer.h"
#include "../idt/idt.h"
#include "../cpu.h"
#include <stddef.h>

// PIT (8253/8254) ports
#define PIT_CHANNEL0 0x40
#define PIT_CMD      0x43

// PIT oscillator runs at 1,193,182 Hz.
// Divisor for TIMER_HZ ticks/sec:
#define PIT_DIVISOR  (1193182 / TIMER_HZ)

static volatile uint64_t ticks = 0;
static void (*tick_callback)(uint64_t) = NULL;

static void timer_handler(registers_t* regs) {
    (void)regs;
    ticks++;
    if (tick_callback) tick_callback(ticks);
}

void timer_init(void) {
    // Program PIT channel 0: lo/hi byte access, mode 3 (square wave), binary
    outb(PIT_CMD,      0x36);
    outb(PIT_CHANNEL0, (uint8_t)(PIT_DIVISOR & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF));

    irq_register_handler(0, timer_handler);
}

uint64_t timer_get_ticks(void) {
    return ticks;
}

void timer_register_tick_callback(void (*cb)(uint64_t)) {
    tick_callback = cb;
}
