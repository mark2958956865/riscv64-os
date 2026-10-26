OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    ddr (rxai!w) : ORIGIN = 0xB0000000, LENGTH = 256M
}

SECTIONS
{
    .text :
    {
        KEEP(*(.text))
    } > ddr
}