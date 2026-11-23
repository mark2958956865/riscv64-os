#include <timeros/types.h>
#include <timeros/stdio.h>
#include <timeros/user_api.h>

#define BUFFER_SIZE 128

#define CR 0x0d
#define LF 0x0a
#define BS 0x08
#define DEL 0x7f

int main(void)
{
    char line[BUFFER_SIZE] = {0};
    size_t len = 0;

    int overflow = 0;
    int skip_lf = 0;

    printf("\nTimer OS user shell\n");
    printf(">> ");

    while (1)
    {
        int c = getchar();

        if (c < 0)
        {
            sys_yield();
            continue;
        }

        // 兼容 CRLF 换行
        if (c == LF && skip_lf)
        {
            skip_lf = 0;
            continue;
        }

        skip_lf = (c == CR);

        // 按下 Enter
        if (c == CR || c == LF)
        {
            printf("\n");

            if (overflow)
            {
                printf("shell: command too long\n");
            }
            else if (len > 0)
            {
                line[len] = '\0';

                // 创建子进程
                int pid = sys_fork();

                if (pid < 0)
                {
                    printf("shell: fork failed\n");
                }
                else if (pid == 0)
                {
                    // 子进程执行指定应用
                    int ret = sys_exec(line);

                    // exec 失败才会返回
                    printf(
                        "shell: exec %s failed, ret=%d\n",
                        line, ret
                    );

                    // 退出失败的子进程
                    sys_exit(127);

                    while (1) {}
                }
                else
                {
                    // 父进程等待指定子进程
                    int child_pid = sys_waitpid(pid);

                    if (child_pid < 0)
                    {
                        printf("shell: wait failed\n");
                    }
                    else
                    {
                        printf(
                            "shell: child %d reaped\n",
                            child_pid
                        );
                    }
                }
            }

            // 清空输入缓冲区
            len = 0;
            overflow = 0;
            line[0] = '\0';

            printf(">> ");
            continue;
        }

        // Backspace / Delete
        if (c == BS || c == DEL)
        {
            if (!overflow && len > 0)
            {
                len--;
                line[len] = '\0';
                printf("\b \b");
            }

            continue;
        }

        // 忽略不可打印字符
        if (c < 32 || c > 126)
        {
            continue;
        }

        if (overflow)
        {
            continue;
        }

        // 检查缓冲区边界
        if (len >= BUFFER_SIZE - 1)
        {
            overflow = 1;
            continue;
        }

        // 保存输入字符
        line[len++] = (char)c;
        line[len] = '\0';

        printf("%c", c);
    }

    return 0;
}
