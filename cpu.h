#ifndef  CPU_H
#define CPU_H

#include <stdint.h>

// Send a byte to a hardware port
static inline void outb(uint16_t port, uint8_t val) {
  __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// Read a byte from a hardware port
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Halt the CPU until the next interupt
static inline void hlt(void) {
  __asm__ volatile ("hlt");
}

// Disable interupts
static inline void cli(void) {
  __asm__ volatile ("cli");
}

// Enable interupts
static inline void sti(void) {
  __asm__ volatile ("sti");
}
#endif
