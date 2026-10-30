#ifndef _OS_H_
#define _OS_H_

#include <stddef.h>
#include <stdarg.h>
#include "sbi.h"
#include "types.h"

int printf(const char *s, ...);
void panic(const char *s);
void sbi_console_putchar(int ch);

#define __NR_write 64
#define __NR_sched_yield 124

size_t sys_write(size_t fd, const char *buf, size_t len);
size_t strlen(const char *str);
void *memcpy(void *dest, const void *src, size_t count);
void trap_init(void);

#endif