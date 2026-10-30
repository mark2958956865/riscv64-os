#include "os.h"
#include "riscv.h"
#include "context.h"

extern void __alltraps(void);
extern void __restore(pt_regs *next);
extern reg_t __SYSCALL(reg_t id, reg_t arg1,
                       reg_t arg2, reg_t arg3);

TrapContext* trap_handler(TrapContext* cx)
{
    reg_t scause = r_scause();
    switch (scause)
    {
        case 8:
            cx->a0 = __SYSCALL(cx->a7, cx->a0,
                       cx->a1, cx->a2);
            cx->sepc += 4;
            break;
        default:
            printf("undefined scause:%lx\n",
           (unsigned long)scause);
            panic("unexpected trap");
    }

    return cx;
}

void trap_init()
{
    /*
     * 设置 trap 时调用函数的基地址
     */
    w_stvec((reg_t)__alltraps);
}
