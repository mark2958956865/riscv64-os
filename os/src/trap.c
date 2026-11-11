#include "os.h"
#include "riscv.h"
#include "context.h"
#include "task.h"

extern void __alltraps(void);
extern void __restore(pt_regs *next);
extern reg_t __SYSCALL(reg_t id, reg_t arg1,
                       reg_t arg2, reg_t arg3);

// TrapContext* trap_handler(TrapContext* cx)
// {
//     reg_t scause = r_scause();
//     switch (scause)
//     {
//         case 8:
//             cx->a0 = __SYSCALL(cx->a7, cx->a0,
//                        cx->a1, cx->a2);
//             cx->sepc += 4;
//             break;
//         default:
//             printf("undefined scause:%lx\n",
//            (unsigned long)scause);
//             panic("unexpected trap");
//     }

//     return cx;
// }

TrapContext* trap_handler(TrapContext* cx)
{
    reg_t scause = r_scause();
    reg_t is_interrupt = scause >> 63;
    reg_t cause_code = scause & ~(1UL << 63);
    if (is_interrupt) {
        switch (cause_code) {
            case 5:
                set_next_trigger();
                schedule();
                break;
            default:
                panic("unexpected interrupt");
        }
    }
    else {
        switch (cause_code) {
            case 8:
                cx->a0 = __SYSCALL(cx->a7, cx->a0,
                               cx->a1, cx->a2);
                cx->sepc += 4;
                break;
            default:
                panic("unexpected exception");
        }
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
