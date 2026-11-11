#include "os.h"

#define PRINTF_BUF_SIZE 1000

int printf(const char *fmt, ...)
{
    char out_buf[PRINTF_BUF_SIZE];

    va_list ap;
    va_start(ap, fmt);

    // 将格式化结果写入缓冲区
    int res = _vsnprintf(out_buf, sizeof(out_buf), fmt, ap);

    va_end(ap);

    // 检查格式化结果是否超出缓冲区
    if (res < 0 || (size_t)res >= sizeof(out_buf)) {
        return -1;
    }

    // 通过系统调用输出
    size_t written = sys_write(1, out_buf, (size_t)res);

    return written == (size_t)res ? res : -1;
}