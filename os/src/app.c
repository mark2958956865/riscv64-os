#include "types.h"
#include <stddef.h>
#include "os.h"
#include "context.h"
#include "task.h"

size_t syscall(size_t id, reg_t arg1,
               reg_t arg2, reg_t arg3)
{
    register reg_t a0 asm("a0") = arg1;
    register reg_t a1 asm("a1") = arg2;
    register reg_t a2 asm("a2") = arg3;
    register reg_t a7 asm("a7") = (reg_t)id;

    asm volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory"
    );

    return (size_t)a0;
}

size_t sys_write(size_t fd, const char* buf, size_t len)
{
    return syscall(__NR_write,(reg_t)fd,(reg_t)buf,(reg_t)len);
}

size_t sys_yield()
{
    return syscall(__NR_sched_yield,0,0,0);
}

uint64_t sys_gettime(void)
{
    return (uint64_t)syscall(__NR_gettimeofday, 0, 0, 0);
}

void task_delay(volatile int count)
{
    count *= 50000;
    while (count--);
}

void task1()
{
    const char *message = "task1 is running!\n";
    int len = strlen(message);
    while (1)
    {
        sys_write(1,message, len);
        task_delay(10000);
        // sys_yield();
    }
}

void task2()
{
    const char *message = "task2 is running!\n";
    int len = strlen(message);
    while (1)
    {
        sys_write(1,message, len);
        task_delay(10000);
        // sys_yield();
    }
}

void task3()
{
    const char *message = "task3 is running!\n";
    int len = strlen(message);
    while (1)
    {
        sys_write(1,message, len);
        task_delay(10000);
        // sys_yield();
    }
    // while (1)
    // {
    //     uint64_t current_timer = sys_gettime();

    //     printf("current_timer: %lu us\n",
    //            (unsigned long)current_timer);

    //     task_delay(100);
    // }
}

void task_init(void)
{
    task_create(task1);
    task_create(task2);
    task_create(task3);
}
