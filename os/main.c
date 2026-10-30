#include "os.h"
#include "task.h"

void os_main(void)
{
    printf("Hello Quard Star OS!\n");

    trap_init();

    task_init();

    run_first_task();
}