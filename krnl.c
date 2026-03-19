void kmain(void);

void kmain(void) {
    volatile char *video_memory = (volatile char*) 0xB8000;
    
    // 1. Clear the screen
    // 80 columns * 25 rows * 2 bytes per character = 4000 bytes
    for (int i = 0; i < 80 * 25 * 2; i += 2) {
        video_memory[i] = ' ';      // Clear character
        video_memory[i+1] = 0x07;  // Light grey on black background
    }

    // 2. Print your message at the top
    const char *str = "nOS is officially alive!";
    unsigned int i = 0;
    unsigned int j = 0;

    while (str[i] != '\0') {
        video_memory[j] = str[i];
        video_memory[j+1] = 0x0F; // Bright white
        i++;
        j += 2;
    }

    while(1); 
}
