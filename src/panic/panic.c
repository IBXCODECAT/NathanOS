#include "panic.h"
#include "../drivers/vga.h"
#include "../cpu.h"

static const char* panic_strings[] = {
#define X(name) #name,
    PANIC_CODE_LIST
#undef X
};

static void print_hex64(uint64_t val) {
    vga_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (val >> shift) & 0xF;
        vga_putc(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}

static void do_panic(panic_code_t code, uint64_t rip, uint64_t err,
                     uint64_t fault_addr, int has_regs) {
    const char* code_str = (code < PANIC_CODE_COUNT) ? panic_strings[code] : "UNKNOWN_PANIC";

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_cls();

    vga_puts("NathanOS has encountered a fatal error and has been shut down to prevent\n");
    vga_puts("damage to your computer.\n");
    vga_puts("\n");
    vga_puts(code_str);
    vga_puts("\n");
    vga_puts("\n");
    vga_puts("If this is the first time you have seen this panic screen, restart your computer.\n");
    vga_puts("If this screen appears again, follow these steps:\n");
    vga_puts("\n");
    vga_puts("Check that any new hardware or drivers are properly installed. If this is a\n");
    vga_puts("new installation, contact your hardware or software manufacturer for updates\n");
    vga_puts("or compatibility information.\n");
    vga_puts("\n");
    vga_puts("If the problem persists, try removing or replacing any recently added hardware\n");
    vga_puts("or device drivers.\n");
    vga_puts("\n");
    vga_puts("Technical information:\n");
    vga_puts("\n");
    vga_puts("*** PANIC: ");
    vga_puts(code_str);
    vga_puts("\n");

    if (has_regs) {
        vga_puts("\n");
        vga_puts("RIP: "); print_hex64(rip);
        vga_puts("  ERR: "); print_hex64(err);
        vga_putc('\n');
        if (fault_addr) {
            vga_puts("FAULT ADDR: "); print_hex64(fault_addr);
            vga_putc('\n');
        }
    }

    cli();
    while (1) hlt();
}

void panic(panic_code_t code) {
    do_panic(code, 0, 0, 0, 0);
}

void panic_ex(panic_code_t code, uint64_t rip, uint64_t err, uint64_t fault_addr) {
    do_panic(code, rip, err, fault_addr, 1);
}
