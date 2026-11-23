#include "types.h"
#include "os.h"
#include <timeros/stdio.h>
#include <timeros/user_api.h>

// /* write 用户程序入口 */
// int main(void)
// {
//     const char message[] = "write app is running!\n";

//     while (1)
//     {
//         // 使用之前实现的用户态 printf()
//         printf("%s", message);
//     }

//     return 0;
// // }
// int main(void)
// {
//     printf("Please input characters:\n");

//     while (1)
//     {
//         int data = getchar();

//         if (data != -1) {
//             printf("%c", data);
//         }
//     }

//     return 0;
// }
int main(void)
{
    printf("[write] before exec\n");

    // 使用 exec 替换当前程序
    int ret = sys_exec("xec");

    // exec 成功后不会继续执行旧程序
    printf("[write] exec failed, ret = %d\n", ret);

    while (1)
    {
        sys_yield();
    }

    return 0;
}