#include "syscall.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../mm/vmm.h"
#include "../cpu.h"
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

    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
    wrmsr(MSR_STAR,   (0x0010ULL << 48) | (0x0008ULL << 32));
    wrmsr(MSR_LSTAR,  (uint64_t)syscall_entry);
    wrmsr(MSR_SFMASK, RFLAGS_IF | RFLAGS_DF);
    wrmsr(MSR_KERNEL_GSBASE, (uint64_t)&cpu_local);
}

void syscall_set_kernel_stack(uint64_t rsp) {
    cpu_local.kernel_rsp = rsp;
}

/* ── syscall implementations ─────────────────────────────────────────── */

static int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count) {
    (void)fd;
    if (count == 0) return 0;
    char *dst = (char *)buf;
    uint64_t n = 0;

    sti();   /* re-enable interrupts so the keyboard IRQ can fill the ring buffer */
    while (n < count) {
        char c;
        while ((c = keyboard_getc()) == 0) hlt();   /* sleep until a key arrives */

        if (c == '\b') {
            if (n > 0) {
                n--;
                vga_putc('\b');   /* move cursor back  */
                vga_putc(' ');    /* blank the cell     */
                vga_putc('\b');   /* move cursor back again */
            }
            continue;
        }

        dst[n++] = c;
        vga_putc(c);          /* echo to screen */
        if (c == '\n') break;
    }
    cli();
    return (int64_t)n;
}

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
    /* Restore the kernel's own address space before halting so we leave
       the CPU in a clean state (relevant once we support multiple processes) */
    vmm_switch(vmm_kernel_cr3());
    for (;;) hlt();
}

uint64_t syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (number) {
        case SYS_READ:  return (uint64_t)sys_read(arg1, arg2, arg3);
        case SYS_WRITE: return (uint64_t)sys_write(arg1, arg2, arg3);
        case SYS_EXIT:  sys_exit(arg1);
    }
    return (uint64_t)-1LL;
}
