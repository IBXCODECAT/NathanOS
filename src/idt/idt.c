#include "idt.h"
#include "../cpu.h"
#include "../panic/panic.h"

// ─── IDT storage ────────────────────────────────────────────────────────────

#define IDT_ENTRIES 48

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

// Kernel code segment selector (gdt64.code = 8 bytes from gdt64 start).
#define KERNEL_CS        0x08
// Present | DPL=0 | 64-bit interrupt gate (type 0xE).
#define IDT_GATE_INT     0x8E

// ─── ISR stubs declared in isr.asm ──────────────────────────────────────────

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void isr32(void); extern void isr33(void); extern void isr34(void);
extern void isr35(void); extern void isr36(void); extern void isr37(void);
extern void isr38(void); extern void isr39(void); extern void isr40(void);
extern void isr41(void); extern void isr42(void); extern void isr43(void);
extern void isr44(void); extern void isr45(void); extern void isr46(void);
extern void isr47(void);

static void* isr_stubs[IDT_ENTRIES] = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47,
};

// ─── IRQ dispatch table ──────────────────────────────────────────────────────

static void (*irq_handlers[16])(registers_t*) = {0};

void irq_register_handler(uint8_t irq, void (*handler)(registers_t*)) {
    if (irq >= 16) panic(PANIC_INVALID_IRQ_NUMBER);
    irq_handlers[irq] = handler;
}

// ─── IDT helpers ────────────────────────────────────────────────────────────

static void idt_set_entry(uint8_t n, void* handler, uint16_t selector, uint8_t type_attr) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[n].selector    = selector;
    idt[n].ist         = 0;
    idt[n].type_attr   = type_attr;
    idt[n].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[n].reserved    = 0;
}

// ─── 8259 PIC remapping ─────────────────────────────────────────────────────
// The BIOS maps IRQ 0-15 to INT 8-15 and 70h-77h, which collides with CPU
// exceptions in protected/long mode.  Remap to INT 32-47 and mask all IRQs.

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

static void pic_remap(void) {
    // Start init sequence (cascade mode)
    outb(PIC1_CMD,  ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD,  ICW1_INIT | ICW1_ICW4);
    // Set vector offsets
    outb(PIC1_DATA, 32);   // master: IRQ 0-7  -> INT 32-39
    outb(PIC2_DATA, 40);   // slave:  IRQ 8-15 -> INT 40-47
    // Tell master there is a slave at IRQ2, tell slave its cascade identity
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);
    // 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    // Unmask IRQ0 (timer) and IRQ1 (keyboard); mask everything else
    outb(PIC1_DATA, 0xFC);
    outb(PIC2_DATA, 0xFF);
}

// ─── IDT init ───────────────────────────────────────────────────────────────

void idt_init(void) {
    pic_remap();

    for (uint8_t i = 0; i < IDT_ENTRIES; i++)
        idt_set_entry(i, isr_stubs[i], KERNEL_CS, IDT_GATE_INT);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}

// ─── Exception → panic code mapping ─────────────────────────────────────────

static const panic_code_t exception_codes[32] = {
    PANIC_DIVIDE_BY_ZERO,                 // 0  #DE
    PANIC_DEBUG_EXCEPTION,                // 1  #DB
    PANIC_NON_MASKABLE_INTERRUPT,         // 2
    PANIC_BREAKPOINT_TRAP,                // 3  #BP
    PANIC_OVERFLOW,                       // 4  #OF
    PANIC_BOUND_RANGE_EXCEEDED,           // 5  #BR
    PANIC_INVALID_OPCODE,                 // 6  #UD
    PANIC_DEVICE_NOT_AVAILABLE,           // 7  #NM
    PANIC_DOUBLE_FAULT,                   // 8  #DF
    PANIC_COPROCESSOR_SEGMENT_OVERRUN,    // 9
    PANIC_INVALID_TSS,                    // 10 #TS
    PANIC_SEGMENT_NOT_PRESENT,            // 11 #NP
    PANIC_STACK_SEGMENT_FAULT,            // 12 #SS
    PANIC_GENERAL_PROTECTION_FAULT,       // 13 #GP
    PANIC_PAGE_FAULT_IN_NON_PAGED_AREA,   // 14 #PF
    PANIC_RESERVED_EXCEPTION,             // 15
    PANIC_X87_FLOATING_POINT_ERROR,       // 16 #MF
    PANIC_ALIGNMENT_CHECK,                // 17 #AC
    PANIC_MACHINE_CHECK,                  // 18 #MC
    PANIC_SIMD_FLOATING_POINT_EXCEPTION,  // 19 #XM
    PANIC_VIRTUALIZATION_EXCEPTION,       // 20 #VE
    PANIC_CONTROL_PROTECTION_EXCEPTION,   // 21 #CP
    PANIC_RESERVED_EXCEPTION,             // 22
    PANIC_RESERVED_EXCEPTION,             // 23
    PANIC_RESERVED_EXCEPTION,             // 24
    PANIC_RESERVED_EXCEPTION,             // 25
    PANIC_RESERVED_EXCEPTION,             // 26
    PANIC_RESERVED_EXCEPTION,             // 27
    PANIC_HYPERVISOR_INJECTION_EXCEPTION, // 28 #HV
    PANIC_VMM_COMMUNICATION_EXCEPTION,    // 29 #VC
    PANIC_SECURITY_EXCEPTION,             // 30 #SX
    PANIC_RESERVED_EXCEPTION,             // 31
};

void isr_handler(registers_t* regs) {
    if (regs->int_num >= 32) {
        uint8_t irq = (uint8_t)(regs->int_num - 32);
        if (irq_handlers[irq]) irq_handlers[irq](regs);
        // Send End of Interrupt to PIC(s)
        if (irq >= 8) outb(PIC2_CMD, 0x20);
        outb(PIC1_CMD, 0x20);
        return;
    }

    uint64_t fault_addr = 0;
    if (regs->int_num == 14)
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));

    panic_ex(exception_codes[regs->int_num], regs->rip, regs->error_code, fault_addr);
}
