OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ram (rwx) : ORIGIN = 0x80200000, LENGTH = 32M
}

SECTIONS
{
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    } > ram

    .rodata : {
        *(.rodata .rodata.*)
    } > ram

    .data : {
        *(.sdata .sdata.*)
        *(.data .data.*)
    } > ram

    .bss (NOLOAD) : {
        . = ALIGN(16);
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
    } > ram
}