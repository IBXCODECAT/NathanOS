#!/bin/bash

# 1. Clean up old build artifacts
echo "Cleaning old files..."
rm -rf *.o krnl nOS/boot/krnl nOS.iso

# 2. Compile the C kernel
# -m32: Compile for 32-bit architecture
# -fno-stack-protector: Disable stack protection (we don't have a library for it yet)
echo "Compiling C kernel..."
gcc -m32 -fno-stack-protector -fno-builtin -c krnl.c -o krnl.o 

# 3. Assemble the bootloader
# -f elf32: Output 32-bit ELF format
echo "Assembling bootloader..."
nasm -f elf32 boot.asm -o boot.o 

# 4. Link the kernel
# -T linker.ld: Use your custom linker script 
echo "Linking..."
ld -m elf_i386 -T linker.ld -o krnl boot.o krnl.o 

# 5. Prepare the ISO structure
echo "Preparing ISO directory..."
mkdir -p nOS/boot/grub
cp krnl nOS/boot/krnl

# 6. Create the grub.cfg if it doesn't exist
if [ ! -f nOS/boot/grub/grub.cfg ]; then
    echo "Creating default grub.cfg..."
    cat << EOF > nOS/boot/grub/grub.cfg
set timeout=0
set default=0
menuentry "nOS" {
    multiboot /boot/krnl
    boot
}
EOF
fi

# 7. Generate the bootable ISO
echo "Generating ISO..."
grub-mkrescue -o nOS.iso nOS

# 8. Cleanup object files
echo "Cleaning up object files..."
rm *.o

echo "Done! You can now run: qemu-system-i386 -cdrom nOS.iso"
