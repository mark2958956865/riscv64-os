#ifndef __TASK_H__
#define __TASK_H__

#include "os.h"
#include "context.h"
#include "address.h"

#define MAX_TASKS 10 

typedef enum TaskState
{
    UnInit,     // 未初始化，任务槽空闲
    Allocated,  // 已分配，正在初始化进程
    Ready,      // 初始化完成，等待调度
    Running,    // 正在运行
    Exited,     // 已退出，等待资源回收
} TaskState;


typedef struct TaskControlBlock
{
    TaskState task_state;             //任务状态
    int pid;                          //进程 PID
    int exit_code;                    //进程退出码
    struct TaskControlBlock *parent;  //父进程指针
    TaskContext task_context;         //任务上下文
    uint64_t trap_cx_ppn;             //Trap 上下文所在物理地址
    uint64_t base_size;               //应用数据大小
    uint64_t kstack;                  //应用内核栈的虚拟地址
    uint64_t ustack;                  //应用用户栈的虚拟地址
    uint64_t entry;                   //应用程序入口地址
    PageTable pagetable;              //应用页表所在物理页
} TaskControlBlock;

void task_create(void (*task_entry)(void));
void run_first_task(void);
void schedule(void);
void task_init(void);
void proc_mapstacks(PageTable *kpgtbl);

void proc_trap(TaskControlBlock *p);
void proc_pagetable(TaskControlBlock *p);
TaskControlBlock *task_create_pt(size_t app_id);
reg_t get_current_trap_cx(void);
reg_t current_user_token(void);
void app_init(size_t app_id);

void procinit(void);
TaskControlBlock *allocproc(void);
TaskControlBlock *current_proc(void);
int __sys_fork(void);

/* 当前进程退出，并切换至其他进程 */
void exit_current_and_run_next(int exit_code);

/* 等待子进程 */
int waitpid(int pid);

/* 回收已退出进程的资源 */
void freeproc(TaskControlBlock *p);

#endif
