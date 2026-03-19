#include "drivers/vga.h"

void kmain(void) {
    vga_init();
    
    vga_puts("nOS Kernel 1.0\n");
    vga_puts("------------------\n");
    vga_puts("VGA Driver: Loaded\n");
    vga_puts("64-bit Long Mode: Active\n");
    
    // Stop the CPU so it doesn't run into random memory
    while(1) { __asm__("hlt"); }
}
