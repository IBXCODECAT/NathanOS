#include "vga.h"
#include "../cpu.h"

static const size_t VGA_WIDTH  = 80;
static const size_t VGA_HEIGHT = 25;

static uint16_t* const VGA_BUFF = (uint16_t*) 0xB8000;


static size_t term_row;
static size_t term_col;
static size_t term_color;

// Helper to create the 16-bit VGA cell (color + char)
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

void vga_init(void) {
    term_row = 0;
    term_col = 0;
    term_color = (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE;
    vga_cls();
}

void vga_cls(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            VGA_BUFF[index] = vga_entry(' ', term_color);
        }
    }
}

// Logic for moving the hardware blinking cursor
static void update_cursor(size_t x, size_t y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void vga_putc(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else {
        const size_t index = term_row * VGA_WIDTH + term_col;
        VGA_BUFF[index] = vga_entry(c, term_color);
        if (++term_col == VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
    }

    // Basic scrolling check
    if (term_row >= VGA_HEIGHT) {
        // Advanced: move memory up here. For now, just reset.
        term_row = 0;
    }
    update_cursor(term_col, term_row);
}

void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}
