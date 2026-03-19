#include "drivers/vga.h"
#include "drivers/keyboard.h"
#include "idt/idt.h"
#include "mm/pmm.h"
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

void kmain(uint32_t mbi_addr) {
    vga_init();

    vga_puts("nOS Kernel 1.0\n");
    vga_puts("------------------\n");
    vga_puts("VGA Driver: Loaded\n");
    vga_puts("64-bit Long Mode: Active\n");

    idt_init();
    vga_puts("IDT: Loaded\n");

    multiboot_info_t* mbi = (multiboot_info_t*)(uint64_t)mbi_addr;
    pmm_init(mbi);
    vga_puts("PMM: Loaded\n");

    vga_puts("PMM: Free pages:  ");
    vga_putu64(pmm_free_pages());
    vga_putc('\n');

    vga_puts("PMM: Total pages: ");
    vga_putu64(pmm_total_pages());
    vga_putc('\n');

    keyboard_init();
    vga_puts("Keyboard: Loaded\n\n");

    vga_puts("Type something: ");
    sti();
    for(;;) { hlt(); }
}
