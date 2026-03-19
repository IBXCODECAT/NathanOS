#!/bin/bash
set -e

# 1. Clean up
rm -rf *.o drivers/*.o idt/*.o krnl nOS.iso

# 2. Assemble the bootloader and ISR stubs
nasm -f elf64 boot.asm -o boot.o
nasm -f elf64 idt/isr.asm -o idt/isr.o

# 3. Compile all C files (including drivers)
# This loop finds every .c file in the current dir and subdirs
for file in $(find . -name "*.c"); do
    echo "Compiling $file..."
    # We use -I. so the compiler can find headers like cpu.h 
    # when drivers/vga.c tries to #include "../cpu.h"
    gcc -m64 -march=x86-64 -ffreestanding -fno-pie -fno-stack-protector \
        -mcmodel=kernel -I. -c "$file" -o "${file%.c}.o"
done

# 4. Link everything
# We explicitly list boot.o first to ensure it's at the start of the binary
echo "Linking..."
ld -m elf_x86_64 -T linker.ld -o krnl boot.o $(find . -name "*.o" ! -name "boot.o")

# 5. Create ISO (assuming nOS folder exists)
mkdir -p nOS/boot/grub
cp krnl nOS/boot/krnl
grub-mkrescue -o nOS.iso nOS

echo "Build Complete!"

# 6. Launch the OS
QEMU_CMD="qemu-system-x86_64 -cdrom nOS.iso"
echo "Running: $QEMU_CMD"
$QEMU_CMD
