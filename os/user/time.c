#include "types.h"
#include <stddef.h>
#include "os.h"
#include <timeros/user_api.h>


// int main(void)
// {
//     uint64_t current_time = 0;
//     uint64_t last_print = 0;
//     while (1)
//     {
//         current_time = sys_gettime();
//         // 每隔约一秒打印一次
//         if (current_time - last_print >= 1000000ULL)
//         {
//             last_print = current_time;
//             printf("current_timer:%lx\n",
//                    (unsigned long)current_time);
//         }
//     }
//     return 0;
// }

// int main(void)
// {
//     uint64_t current_time = 0;
//     uint64_t last_print = sys_gettime();

//     while (1)
//     {
//         current_time = sys_gettime();

//         // 每隔一秒打印一次
//         if (current_time - last_print >= 1000000ULL)
//         {
//             printf("[time] task is running!\n");

//             last_print = current_time;
//         }
//     }

//     return 0;
// }

int main(void)
{
    printf("[fork test] before fork\n");

    // 父进程创建子进程
    int pid = sys_fork();

    if (pid < 0)
    {
        printf("[fork test] fork failed\n");
    }
    else if (pid == 0)
    {
        // 子进程 fork 返回 0
        printf("[child] fork returned 0\n");
    }
    else
    {
        // 父进程返回子进程 PID
        printf("[parent] child pid = %d\n", pid);
    }

    // 当前尚未实现 exit，暂时保持任务运行
    while (1)
    {
        sys_yield();
    }

    return 0;
}