#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Convert a raw PS/2 scancode (set 1) to ASCII, applying active modifiers.
// Must be called for every scancode (including key-releases) so that modifier
// state (shift, etc.) stays accurate.  Returns 0 for non-printable keys.
char kb_scancode_to_ascii(uint8_t scancode);

// Returns the next ASCII character from the keyboard ring buffer, or 0 if empty.
char keyboard_getc(void);

void keyboard_init(void);

#endif
