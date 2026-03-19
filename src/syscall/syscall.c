#include "syscall.h"
#include "../drivers/vga.h"
#include <stdint.h>

#define MSR_EFER          0xC0000080
#define MSR_STAR          0xC0000081
#define MSR_LSTAR         0xC0000082
#define MSR_SFMASK        0xC0000084
#define MSR_KERNEL_GSBASE 0xC0000102

#define EFER_SCE   (1ULL << 0)
#define RFLAGS_IF  (1ULL << 9)
#define RFLAGS_DF  (1ULL << 10)

/* Per-CPU scratch area accessed via GS after swapgs */
typedef struct {
    uint64_t user_rsp;   /* offset 0 — scratch for saving user RSP on syscall entry */
    uint64_t kernel_rsp; /* offset 8 — kernel stack pointer to switch to             */
} __attribute__((packed)) cpu_local_t;

static cpu_local_t cpu_local;
static uint8_t syscall_kstack[4096] __attribute__((aligned(16)));

extern void syscall_entry(void);

static void wrmsr(uint32_t msr, uint64_t val) {
    __asm__ volatile ("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init(void) {
    cpu_local.kernel_rsp = (uint64_t)(syscall_kstack + sizeof(syscall_kstack));

    /* Enable SYSCALL/SYSRET in EFER */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);

    /*
     * STAR[47:32] = kernel CS base → SYSCALL sets CS=0x08, SS=0x10
     * STAR[63:48] = SYSRET base   → SYSRETQ sets CS=(base+16)|3=0x23, SS=(base+8)|3=0x1B
     */
    wrmsr(MSR_STAR,   (0x0010ULL << 48) | (0x0008ULL << 32));
    wrmsr(MSR_LSTAR,  (uint64_t)syscall_entry);
    wrmsr(MSR_SFMASK, RFLAGS_IF | RFLAGS_DF);

    /* swapgs on syscall entry exchanges GSBASE ↔ KERNELGSBASE;
       after the swap GS points here so [gs:0] and [gs:8] reach cpu_local */
    wrmsr(MSR_KERNEL_GSBASE, (uint64_t)&cpu_local);
}

/* ── syscall implementations ─────────────────────────────────────────── */

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    (void)fd;
    const char *s = (const char *)buf;
    for (uint64_t i = 0; i < len; i++)
        vga_putc(s[i]);
    return (int64_t)len;
}

static __attribute__((noreturn)) void sys_exit(uint64_t code) {
    (void)code;
    vga_puts("\n[user exited]\n");
    for (;;) __asm__ volatile ("hlt");
}

/* Called from syscall_entry.asm with args already remapped to SysV convention */
uint64_t syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (number) {
        case SYS_WRITE: return (uint64_t)sys_write(arg1, arg2, arg3);
        case SYS_EXIT:  sys_exit(arg1);
    }
    return (uint64_t)-1LL;
}
