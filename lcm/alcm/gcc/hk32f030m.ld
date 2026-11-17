/*
 * HK32F030MF4P6: 16 KB flash, 2 KB RAM.
 * The last 64 bytes of flash (0x08003FC0) hold the optional battery LUT
 * block written by tools/battery_lut_patch, so code must stay below it.
 */
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 0x3FC0
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 0x800
}

_estack = ORIGIN(RAM) + LENGTH(RAM);
_min_stack = 0x200; /* same as the Keil startup's Stack_Size */

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        *(.glue_7) *(.glue_7t)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx : { *(.ARM.exidx*) } > FLASH

    _sidata = LOADADDR(.data);
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    ASSERT(_ebss + _min_stack <= _estack, "Not enough RAM left for the stack")

    /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note*) }
}
