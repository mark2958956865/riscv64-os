#include "os.h"
#include "task.h"
#include "riscv.h"
#include <stddef.h>
#include <address.h>

extern void __restore(void);
extern void __switch(TaskContext *current, TaskContext *next);
extern void trap_return(void);
extern void trap_handler(void);

#define USER_STACK_SIZE (4096 * 2)
#define KERNEL_STACK_SIZE (4096 * 2)

#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PAGE_SIZE)

uint8_t KernelStack[MAX_TASKS][KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));

uint8_t UserStack[MAX_TASKS][USER_STACK_SIZE]
    __attribute__((aligned(16)));

struct TaskControlBlock tasks[MAX_TASKS];

static int _current = 0;
static int _top = 0;
static int nextpid = 1;

int allocpid(void)
{
    int pid = nextpid;
    nextpid++;
    return pid;
}

struct TaskContext tcx_init(reg_t kstack_ptr) {
    struct TaskContext task_ctx;

    task_ctx.ra = (reg_t)trap_return;
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

void run_first_task(void)
{
    if (_top <= 0) {
        panic("No tasks!");
    }

    // 设置第一个任务为运行状态
    _current = 0;
    tasks[0].task_state = Running;

    struct TaskContext *next_task_cx_ptr = &tasks[0].task_context;

    struct TaskContext unused = {0};

    // 切换到第一个任务
    __switch(&unused, next_task_cx_ptr);

    panic("unreachable in run_first_task!");
}


// void run_first_task()
// {
//     tasks[0].task_state = Running;
//     struct TaskContext *next_task_cx_ptr = &(tasks[0].task_context);
//     struct TaskContext _unused ;

//     __switch(&_unused,next_task_cx_ptr);
//     panic("unreachable in run_first_task!");
// }

// void schedule(void)
// {
//     if (_top <= 0) {
//         panic("No tasks!");
//         return;
//     }
//     int next = -1;
//     for (int i = 1; i < MAX_TASKS; i++) 
//     {
//         int index = (_current + i) % _top;
//         if (tasks[index].task_state == Ready)
//         {
//             next = index;
//             break;
//         }
//     }
//     if (next == -1) 
//     {
//         return;
//     }
//     int current = _current;
//     TaskContext *current_task_cx_ptr = &tasks[current].task_context;
//     TaskContext *next_task_cx_ptr = &tasks[next].task_context;
//     tasks[current].task_state = Ready;
//     tasks[next].task_state = Running;
//     _current = next;
//     __switch(current_task_cx_ptr,next_task_cx_ptr);
// }

void schedule(void)
{
    // 1. 保存当前进程下标
    int current = _current;
    if (current < 0 || current >= MAX_TASKS) {
        panic("Invalid current task!");
    }
    int next = -1;
    // 2. 轮转查找下一个 Ready 进程
    for (int i = 1; i < MAX_TASKS; i++)
    {
        int index = (current + i) % MAX_TASKS;
        if (tasks[index].task_state == Ready)
        {
            next = index;
            break;
        }
    }
    // 3. 没有找到其他可运行进程
    if (next == -1)
    {
        // 当前进程仍在运行，可以继续执行
        if (tasks[current].task_state == Running)
        {
            return;
        }
        // 当前进程已经退出，却没有其他进程可运行
        panic("No runnable tasks!");
    }
    // 4. 获取当前和下一个进程的内核上下文
    TaskContext *current_task_cx_ptr = &tasks[current].task_context;
    TaskContext *next_task_cx_ptr = &tasks[next].task_context;
    // 5. 只有正常运行的进程才设置为 Ready
    if (tasks[current].task_state == Running)
    {
        tasks[current].task_state = Ready;
    }
    // 6. 将下一个进程设置为 Running
    tasks[next].task_state = Running;
    // 7. 更新当前任务编号
    _current = next;
    // 8. 切换内核上下文
    __switch(current_task_cx_ptr, next_task_cx_ptr);
}

/* 为每个任务分配并映射独立的内核栈 */
void proc_mapstacks(PageTable *kpgtbl)
{
    struct TaskControlBlock *p;

    for (p = tasks; p < &tasks[MAX_TASKS]; p++) {
        PhysPageNum ppn = kalloc();
        if (ppn.value == 0) {
            panic("kalloc failed");
        }
        PhysAddr pa = phys_addr_from_phys_page_num(ppn);
        uint64_t va = KSTACK((int)(p - tasks));
        PageTable_map(kpgtbl, virt_addr_from_size_t(va), pa, PAGE_SIZE, PTE_R | PTE_W);
        p->kstack = va + PAGE_SIZE;
    }
}

extern char trampoline[];

void proc_trap(TaskControlBlock *p)
{
    PhysPageNum ppn = kalloc();
    if (ppn.value == 0) {
        panic("proc_trap: kalloc failed");
    }
    PhysAddr pa = phys_addr_from_phys_page_num(ppn);
    p->trap_cx_ppn = pa.value;
    memset(&p->task_context, 0, sizeof(p->task_context));
}

void proc_pagetable(TaskControlBlock *p)
{
    PageTable pagetable;
    PhysPageNum root_ppn = kalloc();
    if (root_ppn.value == 0) {
        panic("proc_pagetable: kalloc failed");
    }
    pagetable.root_ppn = root_ppn;

    PageTable_map(
        &pagetable,
        virt_addr_from_size_t(TRAMPOLINE),
        phys_addr_from_size_t((uintptr_t)trampoline),
        PAGE_SIZE,
        PTE_R | PTE_X
    );
    printf("finish user TRAMPOLINE map!\n");
    PageTable_map(
        &pagetable,
        virt_addr_from_size_t(TRAPFRAME),
        phys_addr_from_size_t(p->trap_cx_ppn),
        PAGE_SIZE,
        PTE_R | PTE_W
    );
    printf("finish user TRAPFRAME map!\n");
    p->pagetable = pagetable;
}

TaskControlBlock *task_create_pt(size_t app_id)
{
    if (app_id >= MAX_TASKS || _top >= MAX_TASKS) {
        panic("task_create_pt: task limit exceeded");
    }
    TaskControlBlock *p = &tasks[app_id];
    proc_trap(p);
    proc_pagetable(p);
    _top++;
    return p;
}

reg_t get_current_trap_cx(void)
{
    return tasks[_current].trap_cx_ppn;
}

reg_t current_user_token(void)
{
    return ((reg_t)8 << 60) |
           tasks[_current].pagetable.root_ppn.value;
}

/* 初始化指定应用程序的 TrapContext 和任务上下文 */
void app_init(size_t app_id)
{
    // 1. 检查应用程序编号
    if (app_id >= (size_t)_top) {
        panic("app_init: invalid app_id");
    }

    TaskControlBlock *p = &tasks[app_id];

    // 2. 确保 TrapContext 和内核栈已经分配
    if (p->trap_cx_ppn == 0 || p->kstack == 0) {
        panic("app_init: task resources not initialized");
    }

    // 3. 获取 TrapContext 地址
    // trap_cx_ppn 实际保存的是物理地址
    TrapContext *cx_ptr =
        (TrapContext *)(uintptr_t)p->trap_cx_ppn;

    // 4. 准备用户态的 sstatus
    reg_t sstatus = r_sstatus();

    sstatus &= ~(1UL << 8);  // SPP = 0，sret 返回 U 模式
    sstatus |=  (1UL << 5);  // SPIE = 1，返回后允许中断

    cx_ptr->sstatus = sstatus;

    // 5. 设置用户程序入口地址
    cx_ptr->sepc = p->entry;

    // 6. 设置用户栈顶虚拟地址
    cx_ptr->sp = p->ustack;

    // 7. 设置内核页表 token
    // 前提：此时已通过 kvminithart() 启用内核页表
    cx_ptr->kernel_satp = r_satp();

    // 8. 设置任务内核栈顶
    cx_ptr->kernel_sp = p->kstack;

    // 9. 设置内核 Trap 处理函数入口
    cx_ptr->trap_handler = (reg_t)trap_handler;

    // 10. 初始化任务切换上下文
    p->task_context = tcx_init(p->kstack);

    // 11. 设置任务为就绪状态
    p->task_state = Ready;
    // 12. 分配进程 PID
    p->pid = allocpid();
    // 13. 初始进程没有由 fork 创建的父进程
    p->parent = NULL;
}

/* 初始化进程控制块数组 */
void procinit(void)
{
    for (int i = 0; i < MAX_TASKS; i++)
    {
        tasks[i].task_state = UnInit;
        tasks[i].pid = 0;
        tasks[i].parent = NULL;
    }
}

// /* 分配一个新的进程控制块 */
// TaskControlBlock *allocproc(void)
// {
//     // 1. 检查任务数量
//     if (_top >= MAX_TASKS) {
//         return NULL;
//     }

//     // 2. 使用下一个空闲任务槽
//     TaskControlBlock *p = &tasks[_top];

//     if (p->task_state != UnInit) {
//         return NULL;
//     }

//     // 3. 先标记为正在创建，暂时不允许调度
//     p->task_state = Allocated;

//     // 4. 分配 PID
//     p->pid = allocpid();
//     p->parent = NULL;

//     // 5. 初始化进程信息
//     p->entry = 0;
//     p->ustack = 0;
//     p->base_size = 0;

//     // 注意：不能清除 p->kstack
//     // 内核栈之前已由 proc_mapstacks 分配

//     // 6. 分配子进程独立的 TrapContext 物理页
//     proc_trap(p);

//     // 7. 创建子进程页表，映射跳板页和 TrapContext
//     proc_pagetable(p);

//     return p;
// }


/* 分配新的进程控制块
 * 优先复用已经回收的任务槽 */
TaskControlBlock *allocproc(void)
{
    TaskControlBlock *p = NULL;
    // 1. 遍历所有任务槽，寻找空闲位置
    // tasks[0] 保留给初始进程
    for (int i = 1; i < MAX_TASKS; i++)
    {
        if (tasks[i].task_state == UnInit)
        {
            p = &tasks[i];
            break;
        }
    }
    // 2. 没有空闲任务槽
    if (p == NULL)
    {
        return NULL;
    }
    // 3. 标记任务槽正在分配
    p->task_state = Allocated;
    // 4. 分配新的 PID
    p->pid = allocpid();
    p->parent = NULL;
    p->exit_code = 0;
    // 5. 初始化进程信息
    p->entry = 0;
    p->ustack = 0;
    p->base_size = 0;
    p->trap_cx_ppn = 0;
    p->pagetable = (PageTable){0};
    p->task_context = (TaskContext){0};
    // 注意：
    // 不清除 p->kstack
    // 内核栈已经由 proc_mapstacks() 分配
    // 6. 为进程分配 TrapContext 物理页
    proc_trap(p);
    // 7. 创建用户页表
    // 映射 trampoline 和 TrapContext
    proc_pagetable(p);
    return p;
}


/* 获取当前正在执行的进程 */
TaskControlBlock *current_proc(void)
{
    return &tasks[_current];
}

/* 创建当前进程的子进程 */
int __sys_fork(void)
{
    // 1. 获取当前父进程
    TaskControlBlock *parent = current_proc();
    // 2. 分配新的子进程控制块
    TaskControlBlock *child = allocproc();
    if (child == NULL) {
        return -1;
    }
    // 3. 复制父进程用户地址空间
    if (uvmcopy(&parent->pagetable, &child->pagetable, parent->base_size) != 0)
    {
        // 当前阶段尚未实现完整的分配失败回滚
        panic("fork: uvmcopy failed");
        return -1;
    }
    // 4. 获取父子进程 TrapContext 的内核访问地址
    TrapContext *parent_cx = (TrapContext *)(uintptr_t)parent->trap_cx_ppn;
    TrapContext *child_cx = (TrapContext *)(uintptr_t)child->trap_cx_ppn;
    // 5. 复制父进程的 TrapContext
    memcpy(child_cx, parent_cx, sizeof(TrapContext));
    // 6. 子进程 fork() 返回值为 0
    child_cx->a0 = 0;
    // 7. 子进程必须使用自己的内核栈
    child_cx->kernel_sp = child->kstack;
    // 8. 继承父进程基本信息
    child->entry = parent->entry;
    child->base_size = parent->base_size;
    child->ustack = parent->ustack;
    child->parent = parent;
    // 9. 初始化子进程内核任务上下文
    // 首次被调度时进入 trap_return()
    child->task_context = tcx_init(child->kstack);
    // 10. 创建完成，允许调度
    child->task_state = Ready;
    // 11. 更新有效任务数量
    // _top++;
    // 12. 父进程返回子进程 PID
    return child->pid;
}


/*将退出进程的子进程交给初始进程
 * p：即将退出的进程 */
static void children_proc_clear(TaskControlBlock *p)
{
    // 初始进程位于 tasks[0]
    TaskControlBlock *initproc = &tasks[0];
    for (int i = 0; i < MAX_TASKS; i++)
    {
        TaskControlBlock *child = &tasks[i];
        // 找到当前进程的子进程
        if (child->task_state != UnInit && child->parent == p)
        {
            // 重新挂到 initproc 下
            child->parent = initproc;
        }
    }
}

/*退出当前进程并调度其他进程
 * exit_code：进程退出码
 * 正常情况下该函数不会返回 */
void exit_current_and_run_next(int exit_code)
{
    // 1. 获取当前进程
    TaskControlBlock *p = current_proc();
    if (p == NULL) {
        panic("exit: no current process");
    }
    // 2. 不允许初始进程退出
    // 当前项目 tasks[0] 是 initproc 所在进程
    if (p == &tasks[0]) {
        panic("init process exiting");
    }
    // 3. 保存退出码
    p->exit_code = exit_code;
    // 4. 将进程标记为已退出、等待回收
    p->task_state = Exited;
    // 5. 将当前进程的子进程交给 initproc
    children_proc_clear(p);
    // 6. 不能在这里直接释放当前内核栈
    // 也不能直接将该任务槽设置为 UnInit
    // 7. 切换到其他可运行进程
    schedule();
    // 退出进程不应该再次恢复执行
    panic("exit: exited process resumed");
}


/* 回收已经退出的进程
 * 必须由其他进程调用，不能回收当前运行进程 */
void freeproc(TaskControlBlock *p)
{
    if (p == NULL || p == &tasks[0]) {
        panic("freeproc: invalid process");
    }
    if (p->task_state != Exited) {
        panic("freeproc: process not exited");
    }
    // 1. 释放用户页表和用户物理页
    // TRAMPOLINE 与 TRAPFRAME 的映射需要
    // 在 proc_freepagetable 中按共享关系处理
    proc_freepagetable(&p->pagetable, p->base_size);
    // 2. 单独释放 TrapContext 物理页
    // 前提：trap_cx_ppn 存储的是物理地址
    if (p->trap_cx_ppn != 0)
    {
        PhysPageNum ppn = floor_phys(phys_addr_from_size_t((size_t)p->trap_cx_ppn));
        kfree(ppn);
    }
    // 3. 清理 TCB
    p->pid = 0;
    p->exit_code = 0;
    p->parent = NULL;
    p->trap_cx_ppn = 0;
    p->base_size = 0;
    p->ustack = 0;
    p->entry = 0;
    p->pagetable = (PageTable){0};
    p->task_context = (TaskContext){0};
    // 4. 保留 kstack
    // 我们之前在 proc_mapstacks() 中
    // 为任务槽预先分配并映射了内核栈
    // 此处不释放，后续可以复用
    // 5. 任务槽恢复为空闲状态
    p->task_state = UnInit;
}


/* 等待子进程结束并回收
 * pid > 0：等待指定子进程
 * pid = -1：等待任意子进程
 * 返回：
 * 成功：子进程 PID
 * 失败：-1 */
int waitpid(int pid)
{
    TaskControlBlock *parent = current_proc();
    if (parent == NULL || pid == 0 || pid < -1) {
        return -1;
    }
    while (1)
    {
        int havekids = 0;
        // 1. 遍历所有任务槽
        for (int i = 0; i < MAX_TASKS; i++)
        {
            TaskControlBlock *child = &tasks[i];
            // 跳过未初始化或非当前进程的子进程
            if (child->task_state == UnInit || child->parent != parent)
            {
                continue;
            }
            // 如果指定 PID，则只检查该进程
            if (pid != -1 && child->pid != pid)
            {
                continue;
            }
            havekids = 1;
            // 2. 子进程尚未退出
            if (child->task_state != Exited)
            {
                continue;
            }
            // 3. 保存 PID 和退出码
            int child_pid = child->pid;
            int exit_code = child->exit_code;
            // 4. 回收子进程
            freeproc(child);
            printk(
                "[wait] pid=%d exit_code=%d\n",
                child_pid, exit_code
            );
            // 5. 返回被回收的 PID
            return child_pid;
        }
        // 6. 没有符合条件的子进程
        if (!havekids)
        {
            return -1;
        }
        // 7. 子进程还未退出，让其他进程执行
        schedule();
        // 调度回来后重新检查子进程
    }
}
