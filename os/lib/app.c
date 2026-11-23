#include "types.h"
#include "os.h"
#include <stddef.h>

/*
 * 用户态通用系统调用接口
 * a7：系统调用编号
 * a0、a1、a2：系统调用参数
 * a0：系统调用返回值
 */
size_t syscall(size_t id, reg_t arg1,
               reg_t arg2, reg_t arg3)
{
    register reg_t a0 asm("a0") = arg1;
    register reg_t a1 asm("a1") = arg2;
    register reg_t a2 asm("a2") = arg3;
    register reg_t a7 asm("a7") = (reg_t)id;

    // 通过 ecall 进入内核
    asm volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory"
    );

    return (size_t)a0;
}

/* 用户态 write 系统调用 */
size_t sys_write(size_t fd,
                 const char *buf,
                 size_t len)
{
    return syscall(__NR_write, (reg_t)fd, (reg_t)buf, (reg_t)len);
}

int sys_read(size_t fd,
             char *buf,
             size_t len)
{
    size_t result = syscall( __NR_read, (reg_t)fd, (reg_t)buf, (reg_t)len);
    if (result == (size_t)-1) {
        return -1;
    }
    return (int)result;
}

int getchar(void)
{
    unsigned char data = 0;
    if (sys_read(0, (char *)&data, 1) != 1) {
        return -1;
    }
    return (int)data;
}

/* 主动让出 CPU */
size_t sys_yield(void)
{
    return syscall(__NR_sched_yield, 0, 0, 0);
}

/* 获取系统时间，单位微秒 */
uint64_t sys_gettime(void)
{
    return (uint64_t)syscall(__NR_gettimeofday, 0, 0, 0);
}

/* 用户态 fork 系统调用 */
int sys_fork(void)
{
    return (int)syscall(__NR_clone, 0, 0, 0);
}

/* 请求执行指定名称的应用程序 */
int sys_exec(const char *name)
{
    return (int)syscall(__NR_execve, (reg_t)name, 0, 0);
}

/* 用户态退出系统调用
 * exit_code：退出状态码
 * 正常执行时不会返回到用户程序 */
int sys_exit(int exit_code)
{
    return (int)syscall(__NR_exit, (reg_t)exit_code, 0, 0);
}


/* 用户态等待子进程
 * pid > 0：等待指定子进程
 * pid = -1：等待任意一个子进程
 * 返回值：
 * >= 0：成功回收的子进程 PID
 * -1：没有符合条件的子进程 */
int sys_waitpid(int pid)
{
    return (int)syscall(__NR_waitid, (reg_t)pid, 0, 0);
}