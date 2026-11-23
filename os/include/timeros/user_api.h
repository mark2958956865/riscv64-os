#ifndef TIMEROS_USER_API_H
#define TIMEROS_USER_API_H

#include <timeros/types.h>
#include <stddef.h>

/* 通用系统调用接口 */
size_t syscall(size_t id, reg_t arg1,
               reg_t arg2, reg_t arg3);

/* 用户态输出 */
size_t sys_write(size_t fd,
                 const char *buf,
                 size_t len);

/* 用户态输入 */
int sys_read(size_t fd,
             char *buf,
             size_t len);

/* 获取一个字符 */
int getchar(void);

/* 主动让出 CPU */
size_t sys_yield(void);

/* 获取系统时间 */
uint64_t sys_gettime(void);

int sys_fork(void);

int sys_exec(const char *name);

int sys_exit(int exit_code);

int sys_waitpid(int pid);
#endif

