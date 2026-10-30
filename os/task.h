#ifndef __TASK_H__
#define __TASK_H__

#include "os.h"
#include "context.h"

typedef enum TaskState
{
    UnInit,  // 未初始化
    Ready,   // 准备运行
    Running, // 正在运行
    Exited,  // 已退出
}TaskState;

typedef struct TaskControlBlock
{
    TaskState task_state;
    TaskContext task_context;
}TaskControlBlock;

void task_create(void (*task_entry)(void));
void run_first_task(void);
void schedule(void);
void task_init(void);

#endif
