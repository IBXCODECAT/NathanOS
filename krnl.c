void kmain(void);

void kmain(void) {
    // 64-bit pointers are now 8 bytes
    volatile char *video_memory = (volatile char*) 0xB8000;
    
    const char *msg = "Successfully entered 64-bit Long Mode!";
    for (int i = 0; msg[i] != '\0'; i++) {
        video_memory[i*2] = msg[i];
        video_memory[i*2 + 1] = 0x0A; // Bright Green
    }

    while(1) { __asm__("hlt"); }
}
