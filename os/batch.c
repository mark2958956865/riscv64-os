#include "types.h"
#include <stddef.h>
#include "riscv.h"
#include "context.h"

#define USER_STACK_SIZE   (4096 * 2)
#define KERNEL_STACK_SIZE (4096 * 2)

extern void __restore(pt_regs *next);
extern void trap_init(void);

uint8_t KernelStack[KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));
// 保证栈空间满足 RISC-V ABI 的 16 字节对齐要求
uint8_t UserStack[USER_STACK_SIZE]
    __attribute__((aligned(16)));

size_t syscall(size_t id, reg_t arg1, reg_t arg2, reg_t arg3) {
    long ret;
    asm volatile (
        "mv a7, %1\n\t"  // Move syscall id to a7 register
        "mv a0, %2\n\t"  // Move args[0] to a0 register
        "mv a1, %3\n\t"  // Move args[1] to a1 register
        "mv a2, %4\n\t"  // Move args[2] to a2 register
        "ecall\n\t"      // Perform syscall
        "mv %0, a0"      // Move return value to 'ret' variable
        : "=r" (ret)
        : "r" (id), "r" (arg1), "r" (arg2), "r" (arg3)
        : "a7", "a0", "a1", "a2", "memory"
    );
    return ret;
}

void testsys() {
    syscall(2,3,4,5);
    syscall(1,1,1,1);
    syscall(1,2,3,4);
    while (1){
    }
}

void app_init_context(void)
{
    // 用户栈顶
    reg_t user_sp = (reg_t)(UserStack + USER_STACK_SIZE);

    // 在内核栈顶部预留 Trap 上下文
    pt_regs *cx = (pt_regs *)
        (KernelStack + KERNEL_STACK_SIZE - sizeof(pt_regs));

    // 清空上下文，避免恢复未初始化的寄存器值
    for (size_t i = 0; i < sizeof(pt_regs) / sizeof(reg_t); i++) {
        ((reg_t *)cx)[i] = 0;
    }

    // 初始化 S 模式 Trap 入口
    trap_init();

    // 清除 SPP 位，使 sret 返回 U 模式
    reg_t sstatus = r_sstatus();
    sstatus &= ~(1UL << 8);

    // 设置第一次启动用户程序所需的上下文
    cx->sepc = (reg_t)testsys;
    cx->sstatus = sstatus;
    cx->sp = user_sp;

    // 恢复上下文，进入用户态
    __restore(cx);
}