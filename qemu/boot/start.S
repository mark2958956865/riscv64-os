
.macro load_data,_src_start,_dst_start,_dst_end
	bgeu	\_dst_start, \_dst_end, 2f
1:
	lw      t0, 0(\_src_start)
	sw      t0, 0(\_dst_start)
	addi    \_src_start, \_src_start, 4
	addi    \_dst_start, \_dst_start, 4
	bltu    \_dst_start, \_dst_end, 1b
2:
	.endm

.section .text
.globl _start
.type _start,@function

_start:

    csrr a0, mhartid

    bnez a0, wait_ready

hart0_init:

    li t3, 0x82100000

    sw zero, 0(t3)
    fence rw, rw

    li t1, 0x10000000
    li t2, 0x42
    sb t2, 0(t1)

    li		a0,	0x202
	slli	a0,	a0, 20      //a0 = 0x20200000
    li		a1,	0x800
	slli	a1,	a1, 20      //a1 = 0x80000000
    li		a2,	0x802
	slli	a2,	a2, 20      //a2 = 0x80200000
	load_data a0,a1,a2

    li		a0,	0x2008
	slli	a0,	a0, 16       //a0 = 0x20080000
    li		a1,	0x822
	slli	a1,	a1, 20       //a1 = 0x82200000
    li		a2,	0x8228
	slli	a2,	a2, 16       //a2 = 0x82280000
	load_data a0,a1,a2

    li t1, 0x10000000
    li t2, 0x43
    sb t2, 0(t1)

    fence rw, rw

    li t3, 0x82100000
    li t4, 1
    sw t4, 0(t3)

    j boot_opensbi

wait_ready:

    li t3, 0x82100000

1:
    lw t4, 0(t3)
    beqz t4, 1b
    fence rw, rw


boot_opensbi:

    fence.i
    csrr a0, mhartid
    li a1, 0x82200000
    li t0, 0x80000000
    jr t0

.size _start, .-_start  
    
.end
