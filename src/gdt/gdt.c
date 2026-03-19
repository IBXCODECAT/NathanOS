#include "gdt.h"
#include "../mm/pmm.h"
#include <stdint.h>

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_desc_t;

typedef struct {
    gdt_entry_t null;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data;   /* 0x18 — must precede user_code for SYSRET */
    gdt_entry_t user_code;   /* 0x20 */
    tss_desc_t  tss;
} __attribute__((packed, aligned(8))) gdt_table_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

extern void gdt_flush(gdt_ptr_t* ptr, uint16_t cs, uint16_t ds);
extern void tss_load(uint16_t sel);

static gdt_table_t gdt;
static tss_t tss;

static gdt_entry_t make_entry(uint8_t access, uint8_t gran) {
    gdt_entry_t e = {0};
    e.access      = access;
    e.granularity = gran;
    return e;
}

void gdt_init(void) {
    /* allocate one page for the kernel interrupt stack */
    uint8_t* kstack = (uint8_t*)pmm_alloc();
    tss.rsp0 = (uint64_t)(kstack + 4096);
    tss.iopb = sizeof(tss_t);

    gdt.null        = make_entry(0x00, 0x00);
    gdt.kernel_code = make_entry(0x9A, 0x20);
    gdt.kernel_data = make_entry(0x92, 0x00);
    gdt.user_data   = make_entry(0xF2, 0x00);
    gdt.user_code   = make_entry(0xFA, 0x20);

    uint64_t base  = (uint64_t)&tss;
    uint32_t limit = (uint32_t)(sizeof(tss_t) - 1);

    tss_desc_t* td    = &gdt.tss;
    td->limit_low     = (uint16_t)(limit & 0xFFFF);
    td->base_low      = (uint16_t)(base & 0xFFFF);
    td->base_mid      = (uint8_t)((base >> 16) & 0xFF);
    td->access        = 0x89;
    td->granularity   = (uint8_t)((limit >> 16) & 0x0F);
    td->base_high     = (uint8_t)((base >> 24) & 0xFF);
    td->base_upper    = (uint32_t)(base >> 32);
    td->reserved      = 0;

    gdt_ptr_t ptr;
    ptr.limit = sizeof(gdt) - 1;
    ptr.base  = (uint64_t)&gdt;

    gdt_flush(&ptr, GDT_KERNEL_CODE, GDT_KERNEL_DATA);
    tss_load(GDT_TSS_SEL);
}

void gdt_set_kernel_stack(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

__attribute__((noreturn)) void ring3_enter(uint64_t entry, uint64_t user_rsp) {
    __asm__ volatile (
        "pushq %[ss]        \n"
        "pushq %[rsp]       \n"
        "pushfq             \n"
        "orq  $0x200, (%%rsp)\n"  /* set IF in saved RFLAGS */
        "pushq %[cs]        \n"
        "pushq %[rip]       \n"
        "iretq              \n"
        :
        : [ss]  "r"((uint64_t)(GDT_USER_DATA)),
          [rsp] "r"(user_rsp),
          [cs]  "r"((uint64_t)(GDT_USER_CODE)),
          [rip] "r"(entry)
        : "memory"
    );
    __builtin_unreachable();
}
