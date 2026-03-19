#include "drivers/vga.h"
#include "idt/idt.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "gdt/gdt.h"
#include "syscall/syscall.h"
#include "proc/proc.h"
#include "panic/panic.h"
#include "cpu.h"

/* ELF binary embedded by objcopy (src/user/user.elf → user_elf.o) */
extern uint8_t _binary_user_elf_start[];

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

    vga_puts("User: Starting process\n\n");

    /* Create isolated address space, load ELF, drop into ring 3 */
    process_run_elf(_binary_user_elf_start);
}
