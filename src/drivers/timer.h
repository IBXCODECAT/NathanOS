#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_HZ 100   // ticks per second

void     timer_init(void);
uint64_t timer_get_ticks(void);

// Register a function to be called on every timer tick.
// Only one callback is supported; calling this again replaces the previous one.
void timer_register_tick_callback(void (*cb)(uint64_t ticks));

#endif
