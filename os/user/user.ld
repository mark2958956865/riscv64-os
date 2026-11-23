
OUTPUT_ARCH(riscv)
ENTRY(main)

/* 用户程序虚拟地址起点 */
BASE_ADDRESS = 0x10000;

SECTIONS
{
    . = BASE_ADDRESS;

    /* 用户程序代码段 */
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }

    . = ALIGN(0x1000);

    /* 只读数据段 */
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(0x1000);

    /* 可读写数据段 */
    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    }

    . = ALIGN(0x1000);

    /* 未初始化数据段 */
    .bss (NOLOAD) : {
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
    }

    /* 用户程序占用的虚拟地址范围结束 */
    . = ALIGN(0x1000);
    user_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.eh_frame.*)
    }
}
