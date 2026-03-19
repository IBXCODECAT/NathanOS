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

void vga_set_color(uint8_t fg, uint8_t bg) {
    term_color = (bg << 4) | (fg & 0x0F);
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
    term_row = 0;
    term_col = 0;
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

    // Scroll up one line when the cursor passes the last row
    if (term_row >= VGA_HEIGHT) {
        // Move every row up by one
        for (size_t y = 1; y < VGA_HEIGHT; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                VGA_BUFF[(y - 1) * VGA_WIDTH + x] = VGA_BUFF[y * VGA_WIDTH + x];
            }
        }
        // Clear the new bottom row
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFF[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
        }
        term_row = VGA_HEIGHT - 1;
    }
    update_cursor(term_col, term_row);
}

void vga_puts(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putc(str[i]);
    }
}
