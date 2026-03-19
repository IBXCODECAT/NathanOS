#ifndef GDT_H
#define GDT_H
#include <stdint.h>

#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_DATA    0x1B   /* 0x18 | RPL=3 — data before code; required for SYSRET */
#define GDT_USER_CODE    0x23   /* 0x20 | RPL=3 */
#define GDT_TSS_SEL      0x28

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
} __attribute__((packed)) tss_t;

void gdt_init(void);
void gdt_set_kernel_stack(uint64_t rsp0);
__attribute__((noreturn)) void ring3_enter(uint64_t entry, uint64_t user_rsp);

#endif
