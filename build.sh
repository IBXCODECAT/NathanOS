#!/bin/bash
set -e

# 1. Clean up
rm -rf src/*.o src/drivers/*.o src/idt/*.o src/mm/*.o krnl nOS.iso

# 2. Assemble the bootloader and ISR stubs
nasm -f elf64 src/boot.asm -o src/boot.o
nasm -f elf64 src/idt/isr.asm -o src/idt/isr.o

# 3. Compile all C files (including drivers)
# This loop finds every .c file under src/
for file in $(find src/ -name "*.c"); do
    echo "Compiling $file..."
    # -Isrc/ so the compiler can find cpu.h, drivers/vga.h, etc.
    gcc -m64 -march=x86-64 -ffreestanding -fno-pie -fno-stack-protector \
        -mcmodel=kernel -Isrc/ -c "$file" -o "${file%.c}.o"
done

# 4. Link everything
# We explicitly list boot.o first to ensure it's at the start of the binary
echo "Linking..."
ld -m elf_x86_64 -T src/linker.ld -o krnl src/boot.o $(find src/ -name "*.o" ! -name "boot.o")

# 5. Create ISO
mkdir -p nOS/boot/grub
cp krnl nOS/boot/krnl
grub-mkrescue -o nOS.iso nOS

echo "Build Complete!"

# 6. Launch the OS
QEMU_CMD="qemu-system-x86_64 -cdrom nOS.iso"
echo "Running: $QEMU_CMD"
$QEMU_CMD
