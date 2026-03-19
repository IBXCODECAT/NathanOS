#include "keyboard.h"
#include "timer.h"
#include "vga.h"
#include "../idt/idt.h"
#include "../panic/panic.h"
#include "../cpu.h"

#define KB_DATA_PORT 0x60

#define SC_ESC         0x01
#define SC_ESC_REL     0x81
#define SC_LSHIFT      0x2A
#define SC_RSHIFT      0x36
#define SC_LSHIFT_REL  (SC_LSHIFT | 0x80)
#define SC_RSHIFT_REL  (SC_RSHIFT | 0x80)

#define ESC_HOLD_TICKS (10 * TIMER_HZ)   // 10 seconds

// PS/2 scancode set 1 — unshifted (0 = non-printable)
static const char scancode_normal[128] = {
//  0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
    0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',  '=', '\b', '\t', // 0x00
   'q',  'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',  ']', '\n',   0,  'a',  's', // 0x10
   'd',  'f',  'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0,  '\\', 'z', 'x', 'c',  'v', // 0x20
   'b',  'n',  'm', ',', '.', '/',   0,  '*',   0,  ' ',   0,                             // 0x30
};

// PS/2 scancode set 1 — shifted
static const char scancode_shifted[128] = {
//  0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
    0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_',  '+', '\b', '\t', // 0x00
   'Q',  'W',  'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{',  '}', '\n',   0,  'A',  'S', // 0x10
   'D',  'F',  'G', 'H', 'J', 'K', 'L', ':', '"',  '~',  0,   '|', 'Z', 'X', 'C',  'V', // 0x20
   'B',  'N',  'M', '<', '>', '?',   0,  '*',   0,  ' ',   0,                             // 0x30
};

static int      shift_held      = 0;
static int      esc_held        = 0;
static uint64_t esc_start_tick  = 0;

// Called by the timer on every tick; checks if Esc has been held long enough.
static void keyboard_on_tick(uint64_t ticks) {
    if (esc_held && ticks - esc_start_tick >= ESC_HOLD_TICKS)
        panic(PANIC_MANUALLY_INITIATED_PANIC);
}

char kb_scancode_to_ascii(uint8_t scancode) {
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_held = 1;
        return 0;
    }
    if (scancode == SC_LSHIFT_REL || scancode == SC_RSHIFT_REL) {
        shift_held = 0;
        return 0;
    }
    if (scancode & 0x80) return 0;   // any other key release
    if (scancode >= 128)  return 0;
    return shift_held ? scancode_shifted[scancode] : scancode_normal[scancode];
}

static void keyboard_handler(registers_t* regs) {
    (void)regs;
    uint8_t scancode = inb(KB_DATA_PORT);

    // Track Esc hold time.  Guard against auto-repeat re-setting the start tick.
    if (scancode == SC_ESC) {
        if (!esc_held) {
            esc_held       = 1;
            esc_start_tick = timer_get_ticks();
        }
    } else if (scancode == SC_ESC_REL) {
        esc_held = 0;
    }

    char c = kb_scancode_to_ascii(scancode);
    if (c) vga_putc(c);
}

void keyboard_init(void) {
    irq_register_handler(1, keyboard_handler);
    timer_register_tick_callback(keyboard_on_tick);
}
