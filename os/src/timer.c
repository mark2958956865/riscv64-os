#include "os.h"
#include "riscv.h"
#define CLOCK_FREQ 10000000
#define TICKS_PER_SEC 1000

/* 设置下次时钟中断的cnt值 */
void set_next_trigger()
{
    sbi_set_timer(r_mtime() + CLOCK_FREQ / TICKS_PER_SEC);
}

/* 开启S模式下的时钟中断 */
void timer_init()
{
    // reg_t sstatus = r_sstatus();
    // sstatus |= (1L << 1);
    // w_sstatus(sstatus);
    reg_t sie = r_sie();
    sie |= (1UL << 5);    // 开启 S 模式时钟中断
    w_sie(sie);
    set_next_trigger();
}

/* 以us为单位返回时间 */
uint64_t get_time_us(void)
{
    uint64_t ticks = r_mtime();
    return ticks / (CLOCK_FREQ / 1000000ULL);
}
