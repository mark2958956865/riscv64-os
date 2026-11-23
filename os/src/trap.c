#include "os.h"
#include "riscv.h"
#include "context.h"
#include "task.h"
#include "address.h"

extern void __alltraps(void);
extern void __restore(pt_regs *next);
extern reg_t __SYSCALL(reg_t id, reg_t arg1,
                       reg_t arg2, reg_t arg3);

void trap_return(void);

void trap_from_kernel(void)
{
    panic("a trap from kernel!\n");
}

void set_kernel_trap_entry(void)
{
    w_stvec((reg_t)trap_from_kernel);
}

void set_user_trap_entry(void)
{
    w_stvec((reg_t)TRAMPOLINE);
}

void trap_handler(void)
{
    set_kernel_trap_entry();
    TrapContext *cx =(TrapContext *)(uintptr_t)get_current_trap_cx();
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
                break;
        }
    }
    else {
        switch (cause_code) {
            case 8:
                cx->sepc += 4;
                reg_t id = cx->a7;
                reg_t arg1 = cx->a0;
                reg_t arg2 = cx->a1;
                reg_t arg3 = cx->a2;
                reg_t result = __SYSCALL(id, arg1, arg2, arg3);
                cx = (TrapContext *)(uintptr_t) get_current_trap_cx();
                cx->a0 = result;
                break;
            default:
                panic("unexpected exception");
                break;
        }
    }   
    trap_return();
}

void trap_return(void)
{
    
    set_user_trap_entry();

    reg_t trap_cx_ptr = TRAPFRAME;

    reg_t user_satp = current_user_token();

    reg_t restore_va =(reg_t)__restore - (reg_t)__alltraps + TRAMPOLINE;

    asm volatile (
            "fence.i\n\t"
            "mv a0, %0\n\t"
            "mv a1, %1\n\t"
            "jr %2\n\t"
            :
            : "r"(trap_cx_ptr),
            "r"(user_satp),
            "r"(restore_va)
            : "a0", "a1", "memory"
        );
}

void trap_init(void)
{
    set_kernel_trap_entry();
}
