#include "drivers/vga.h"
#include "idt/idt.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "gdt/gdt.h"
#include "syscall/syscall.h"
#include "panic/panic.h"

/* Flat binary embedded by objcopy (src/user/user.bin → user_bin.o) */
extern uint8_t _binary_user_bin_start[];
extern uint8_t _binary_user_bin_end[];

/* Virtual addresses for user code and stack — above the 64MB identity map */
#define USER_CODE_BASE  0x4000000ULL
#define USER_STACK_BASE 0x4010000ULL  /* one 4KB page; RSP starts at top */

static void vga_putu64(uint64_t n) {
    if (n == 0) { vga_putc('0'); return; }
    char buf[20];
    int i = 0;
    while (n > 0) {
        buf[i++] = '0' + (int)(n % 10);
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--)
        vga_putc(buf[j]);
}

void kmain(uint32_t mbi_addr) {
    vga_init();

    vga_puts("nOS Kernel 1.0\n");
    vga_puts("------------------\n");
    vga_puts("VGA Driver: Loaded\n");
    vga_puts("64-bit Long Mode: Active\n");

    idt_init();
    vga_puts("IDT: Loaded\n");

    if (!mbi_addr) panic(PANIC_NO_MULTIBOOT_INFO);
    multiboot_info_t* mbi = (multiboot_info_t*)(uint64_t)mbi_addr;
    pmm_init(mbi);
    vga_puts("PMM: Loaded\n");

    vga_puts("PMM: Free pages:  ");
    vga_putu64(pmm_free_pages());
    vga_putc('\n');

    vga_puts("PMM: Total pages: ");
    vga_putu64(pmm_total_pages());
    vga_putc('\n');

    heap_init();
    vga_puts("Heap: Loaded (");
    vga_putu64(heap_free_bytes() / 1024);
    vga_puts(" KB free)\n");

    vmm_init();
    vga_puts("VMM: Loaded\n");

    // VMM smoke test
    uint64_t phys = (uint64_t)pmm_alloc();
    uint64_t virt = 0x4001000;
    vmm_map(virt, phys, VMM_KERNEL_DATA);
    volatile uint64_t* p = (volatile uint64_t*)virt;
    *p = 0xDEADBEEFCAFEBABEULL;
    if (*p != 0xDEADBEEFCAFEBABEULL) panic(PANIC_MANUALLY_INITIATED_PANIC);
    vmm_unmap(virt);
    pmm_free((void*)phys);
    vga_puts("VMM: Smoke test passed\n");

    gdt_init();
    vga_puts("GDT: Loaded\n");

    syscall_init();
    vga_puts("Syscall: Loaded\n");

    /* ── Load user binary into ring-3 accessible memory ────────────── */
    uint64_t bin_size = (uint64_t)(_binary_user_bin_end - _binary_user_bin_start);

    /* Map enough 4KB pages to hold the binary (writable so we can copy in) */
    uint64_t npages = (bin_size + 0xFFF) >> 12;
    if (npages == 0) npages = 1;
    for (uint64_t i = 0; i < npages; i++) {
        void* pg = pmm_alloc();
        vmm_map(USER_CODE_BASE + i * 0x1000, (uint64_t)pg, VMM_USER_DATA);
    }

    /* Copy binary bytes to the mapped virtual address */
    uint8_t* dst = (uint8_t*)USER_CODE_BASE;
    for (uint64_t i = 0; i < bin_size; i++)
        dst[i] = _binary_user_bin_start[i];

    /* Map one page for the user stack */
    void* stack_pg = pmm_alloc();
    vmm_map(USER_STACK_BASE, (uint64_t)stack_pg, VMM_USER_DATA);

    vga_puts("User: Loaded\n\n");

    /* Drop into ring 3 — noreturn; sys_exit will hlt */
    ring3_enter(USER_CODE_BASE, USER_STACK_BASE + 0x1000);
}
