/* LCM bootloader: first 1 KB of flash. The app starts at 0x08000400. */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 0x400
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x800
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
    /* no initialised data is used
     * (nothing to copy at startup) */
    .data (NOLOAD) : { *(.data*) } > RAM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > RAM
}
