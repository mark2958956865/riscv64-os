#include "os.h"
#include "task.h"

void os_main(void)
{
    printk("Hello Quard Star OS!\n");

    trap_init();

    task_init();

    timer_init();

    run_first_task();
} 