#include "os.h"
#include "types.h"
#include "task.h"

static void __sys_yield(void);

static reg_t __sys_write(reg_t fd, reg_t buf, reg_t len)
{
    if (fd != 1) {
        return (reg_t)-1;
    }

    const char *data = (const char *)buf;

    for (reg_t i = 0; i < len; i++) {
        printf("%c", data[i]);
    }

    return len;
}

reg_t __SYSCALL(reg_t id, reg_t arg1,reg_t arg2, reg_t arg3)
{
    switch (id) {
        case __NR_write:
            return __sys_write(arg1, arg2, arg3);
        case __NR_sched_yield:
            __sys_yield();
            return 0;
        default:
            printf("Unsupported syscall id: %lu\n",
                   (unsigned long)id);
            return (reg_t)-1;
    }
}

static void __sys_yield(void)
{
    schedule();
}