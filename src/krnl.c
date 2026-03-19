#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "idt/idt.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "gdt/gdt.h"
#include "proc/task.h"
#include "panic/panic.h"
#include "cpu.h"

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

static void task_a(void) {
    for (;;) {
        vga_puts("A");
        for (volatile int i = 0; i < 5000000; i++);
    }
}

static void task_b(void) {
    for (;;) {
        vga_puts("B");
        for (volatile int i = 0; i < 5000000; i++);
    }
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

    gdt_init();
    vga_puts("GDT: Loaded\n");

    // VMM smoke test: map a fresh PMM page at a new virtual address
    uint64_t phys = (uint64_t)pmm_alloc();
    uint64_t virt = 0x4001000;  // above 64MB identity map
    vmm_map(virt, phys, VMM_KERNEL_DATA);

    volatile uint64_t* p = (volatile uint64_t*)virt;
    *p = 0xDEADBEEFCAFEBABEULL;
    if (*p != 0xDEADBEEFCAFEBABEULL) panic(PANIC_MANUALLY_INITIATED_PANIC);

    vmm_unmap(virt);
    pmm_free((void*)phys);
    vga_puts("VMM: Smoke test passed\n");

    timer_init();
    vga_puts("Timer: Loaded\n");

    keyboard_init();
    vga_puts("Keyboard: Loaded\n");

    sched_init();
    task_create(task_a);
    task_create(task_b);
    vga_puts("Scheduler: Loaded\n\n");

    vga_puts("Tasks running (A/B interleave, keyboard still works):\n");
    sti();
    for(;;) { hlt(); }
}
