#ifndef TOS_STDIO_H
#define TOS_STDIO_H

#include <stddef.h>
#include <stdarg.h>

// 公共格式化函数
int _vsnprintf(char *out, size_t n,
               const char *fmt, va_list ap);

// 内核态输出
int printk(const char *fmt, ...);
void panic(const char *msg);

// 用户态输出
int printf(const char *fmt, ...);

// 文件描述符
typedef enum {
    stdin  = 0,
    stdout = 1,
    stderr = 2
} std_fd_t;

#endif