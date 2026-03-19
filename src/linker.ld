OUTPUT_FORMAT(elf64-x86-64)
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
    .bss    : { 
        *(.bss) 
        . = ALIGN(4096); 
    }
    
    /DISCARD/ : { *(.comment) *(.eh_frame) }
}
