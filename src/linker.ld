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
        *(COMMON)
    }
    _kernel_end = .;

    /DISCARD/ : { *(.comment) *(.eh_frame) }
}
