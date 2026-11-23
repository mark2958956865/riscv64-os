#include <timeros/types.h>
#include <timeros/stdio.h>
#include <timeros/user_api.h>

/*
 * 系统初始用户进程
 * 启动后将自身替换为 user_shell
 */
int main(void)
{
    // 1. 使用 exec 启动 Shell
    int ret = sys_exec("user_shell");

    // 2. exec 成功后不会返回
    // 只有 exec 失败才会执行下面的代码
    printf("initproc: exec user_shell failed, ret=%d\n", ret);

    // 3. 初始进程不能退出
    // 暂时保持存活，避免退出初始化进程
    while (1)
    {
        sys_yield();
    }

    return 0;
}
