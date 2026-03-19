ENTRY(start)
SECTIONS
{
  . = 0x100000;
  .text : {
    *(.mboot)    /* Ensure header is at the start */
    *(.text)
  }
  .data : { *(.data) }
  .bss  : { *(.bss)  }
}
