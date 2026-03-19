OUTPUT_FORMAT(elf64-x86-64) /* Changed from elf32-i386 */
ENTRY(start)

SECTIONS
{
    . = 0x100000;
    .text : {
        KEEP(*(.mboot))
        *(.text)
    }
    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { *(.bss) }
    
    /DISCARD/ : { *(.comment) *(.eh_frame) }
}
