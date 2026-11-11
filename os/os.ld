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
        . = ALIGN(0X1000);
        etext = .;
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

. = ALIGN(0x1000);
kernelend = .;

}