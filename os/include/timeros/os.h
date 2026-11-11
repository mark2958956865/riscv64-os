#ifndef _OS_H_
#define _OS_H_

#include <stddef.h>
#include <stdarg.h>
#include "stdio.h"
#include "sbi.h"
#include "types.h"

void sbi_console_putchar(int ch);

#define __NR_write 64
#define __NR_sched_yield 124
#define __NR_gettimeofday 169

size_t sys_write(size_t fd, const char *buf, size_t len);
size_t strlen(const char *str);
void *memcpy(void *dest, const void *src, size_t count);
void *memset(void *dest, int ch, size_t count);
void trap_init(void);
void set_next_trigger(void);
void timer_init(void);
size_t syscall(size_t id, reg_t arg1,
               reg_t arg2, reg_t arg3);
reg_t __SYSCALL(reg_t id, reg_t arg1,
                reg_t arg2, reg_t arg3);

uint64_t get_time_us(void);

#endif