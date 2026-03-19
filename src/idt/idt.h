#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// A single 64-bit IDT gate descriptor (16 bytes).
typedef struct {
    uint16_t offset_low;   // handler VA bits 0-15
    uint16_t selector;     // code segment selector (0x08 = kernel CS)
    uint8_t  ist;          // interrupt stack table offset (bits 2:0), rest 0
    uint8_t  type_attr;    // present | DPL | gate type
    uint16_t offset_mid;   // handler VA bits 16-31
    uint32_t offset_high;  // handler VA bits 32-63
    uint32_t reserved;     // must be 0
} __attribute__((packed)) idt_entry_t;

// Loaded into IDTR via lidt.
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

// Snapshot of all registers at the time of an interrupt.
// Mirrors the stack layout built by isr.asm: the CPU pushes
// SS/RSP/RFLAGS/CS/RIP (and an error code where applicable),
// the stub pushes int_num (and a dummy 0 for error-less exceptions),
// then isr_common pushes the GPRs in order rax..r15.
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_num, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

void idt_init(void);

// Register a C handler for hardware IRQ n (0–15).
// Called automatically when that IRQ fires; EOI is sent by the IDT layer.
void irq_register_handler(uint8_t irq, void (*handler)(registers_t*));

// Called from isr.asm for every exception/interrupt.
void isr_handler(registers_t* regs);

#endif
