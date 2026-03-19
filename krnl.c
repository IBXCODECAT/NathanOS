#include "drivers/vga.h"
#include "idt/idt.h"

void kmain(void) {
    vga_init();

    vga_puts("nOS Kernel 1.0\n");
    vga_puts("------------------\n");
    vga_puts("VGA Driver: Loaded\n");
    vga_puts("64-bit Long Mode: Active\n");

    idt_init();
    vga_puts("IDT: Loaded\n");

    while(1) { __asm__("hlt"); }
}
