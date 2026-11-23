#include <timeros/types.h>
#include <timeros/stdio.h>
#include <timeros/user_api.h>

// int main(void)
// {
//     printf("[xec] new program is running!\n");

//     // 暂时不退出，因为 exit 尚未实现
//     while (1)
//     {
//         sys_yield();
//     }

//     return 0;
// }

int main(void)
{
    printf("[xec] process started\n");
    // 执行少量循环，测试任务正常运行
    for (int i = 0; i < 5; i++)
    {
        printf("[xec] step = %d\n", i);
        // 主动让出 CPU
        sys_yield();
    }
    printf("[xec] exiting, code = 50\n");
    // 主动退出当前进程
    sys_exit(50);
    // 正常情况下不会执行到这里
    printf("[xec] ERROR: returned from exit!\n");
    while (1)
    {
        sys_yield();
    }
    return 0;
}