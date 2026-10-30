#include "os.h"
#include "task.h"
#include "riscv.h"
#include <stddef.h>

extern void __restore(void);
extern void __switch(TaskContext *current, TaskContext *next);

#define USER_STACK_SIZE (4096 * 2)
#define KERNEL_STACK_SIZE (4096 * 2)
#define MAX_TASKS 10   /* 操作系统支持的最大任务数量 */

uint8_t KernelStack[MAX_TASKS][KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));

uint8_t UserStack[MAX_TASKS][USER_STACK_SIZE]
    __attribute__((aligned(16)));

struct TaskControlBlock tasks[MAX_TASKS];

static int _current = 0;
static int _top = 0;

struct TaskContext tcx_init(reg_t kstack_ptr) {
    struct TaskContext task_ctx;

    task_ctx.ra = (reg_t)__restore;;
    task_ctx.sp = kstack_ptr;
    task_ctx.s0 = 0;
    task_ctx.s1 = 0;
    task_ctx.s2 = 0;
    task_ctx.s3 = 0;
    task_ctx.s4 = 0;
    task_ctx.s5 = 0;
    task_ctx.s6 = 0;
    task_ctx.s7 = 0;
    task_ctx.s8 = 0;
    task_ctx.s9 = 0;
    task_ctx.s10 = 0;
    task_ctx.s11 = 0;

    return task_ctx;
}

void task_create(void(*task_entry)(void))
{
    if (_top < MAX_TASKS)
    {
        TrapContext *cx_ptr = (TrapContext *)
            (KernelStack[_top] + KERNEL_STACK_SIZE - sizeof(TrapContext));
        reg_t user_sp =(reg_t)
            (UserStack[_top] + USER_STACK_SIZE);
        reg_t sstatus = r_sstatus();

        sstatus &= ~(1UL << 8);
        for (size_t i = 0;i < sizeof(TrapContext) / sizeof(reg_t);i++)
        {
            ((reg_t *)cx_ptr)[i] = 0;
        }
        cx_ptr->sepc = (reg_t)task_entry;
        cx_ptr->sstatus = sstatus;
        cx_ptr->sp = user_sp;
        tasks[_top].task_context = tcx_init((reg_t)cx_ptr);
        tasks[_top].task_state = Ready;
        _top++;

    }
}

void run_first_task()
{
    tasks[0].task_state = Running;
    struct TaskContext *next_task_cx_ptr = &(tasks[0].task_context);
    struct TaskContext _unused ;

    __switch(&_unused,next_task_cx_ptr);
    panic("unreachable in run_first_task!");
}

void schedule(void)
{
    if (_top <= 0) {
        panic("No tasks!");
        return;
    }
    int next = -1;
    for (int i = 1; i < _top; i++) 
    {
        int index = (_current + i) % _top;
        if (tasks[index].task_state == Ready)
        {
            next = index;
            break;
        }
    }
    if (next == -1) 
    {
        return;
    }
    int current = _current;
    TaskContext *current_task_cx_ptr = &tasks[current].task_context;
    TaskContext *next_task_cx_ptr = &tasks[next].task_context;
    tasks[current].task_state = Ready;
    tasks[next].task_state = Running;
    _current = next;
    __switch(current_task_cx_ptr,next_task_cx_ptr);
}