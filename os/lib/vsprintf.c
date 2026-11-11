#include "os.h"
#include <stdint.h>
#include <limits.h>

/* 向缓冲区写入一个字符，同时统计总长度 */
static void buf_putc(char *out, size_t n,
                     size_t *pos, char ch)
{
    if (out && n > 0 && *pos < n - 1) {
        out[*pos] = ch;
    }

    ++(*pos);
}

static void buf_puts(char *out, size_t n,
                     size_t *pos, const char *s)
{
    if (!s) {
        s = "(null)";
    }

    while (*s) {
        buf_putc(out, n, pos, *s++);
    }
}

/* 将无符号整数转换为十进制或十六进制 */
static void buf_put_unsigned(char *out, size_t n,
                             size_t *pos,
                             unsigned long value,
                             unsigned base,
                             unsigned min_width)
{
    const char digits[] = "0123456789abcdef";
    char reversed[3 * sizeof(unsigned long)];
    unsigned count = 0;

    /* 取余得到数字，先逆序保存 */
    do {
        reversed[count++] = digits[value % base];
        value /= base;
    } while (value != 0);

    /* 不足指定宽度时补零 */
    while (count < min_width) {
        reversed[count++] = '0';
    }

    /* 逆序输出，恢复正确数字顺序 */
    while (count > 0) {
        buf_putc(out, n, pos, reversed[--count]);
    }
}

/* 将有符号整数转换为十进制 */
static void buf_put_signed(char *out, size_t n,
                           size_t *pos, long value)
{
    unsigned long magnitude = (unsigned long)value;

    if (value < 0) {
        buf_putc(out, n, pos, '-');

        /* 使用无符号运算，避免 LONG_MIN 取反溢出 */
        magnitude = 0UL - magnitude;
    }

    buf_put_unsigned(out, n, pos, magnitude, 10, 0);
}

int _vsnprintf(char *out, size_t n,
                      const char *fmt, va_list ap)
{
    size_t pos = 0;

    while (*fmt) {

        /* 普通字符直接输出 */
        if (*fmt != '%') {
            buf_putc(out, n, &pos, *fmt++);
            continue;
        }

        /* 跳过 % */
        ++fmt;

        if (*fmt == '\0') {
            buf_putc(out, n, &pos, '%');
            break;
        }

        /* 判断是否使用 long 类型 */
        int longarg = 0;

        if (*fmt == 'l') {
            longarg = 1;
            ++fmt;
        }

        if (*fmt == '\0') {
            buf_putc(out, n, &pos, '%');
            buf_putc(out, n, &pos, 'l');
            break;
        }

        switch (*fmt) {

        /* %%：输出百分号 */
        case '%':
            buf_putc(out, n, &pos, '%');
            break;

        /* %c：单个字符 */
        case 'c': {
            int ch = va_arg(ap, int);
            buf_putc(out, n, &pos, (char)ch);
            break;
        }

        /* %s：字符串 */
        case 's': {
            const char *s = va_arg(ap, const char *);
            buf_puts(out, n, &pos, s);
            break;
        }

        /* %d / %ld：有符号十进制整数 */
        case 'd': {
            long value = longarg ?
                va_arg(ap, long) :
                va_arg(ap, int);

            buf_put_signed(out, n, &pos, value);
            break;
        }

        /* %u / %lu：无符号十进制整数 */
        case 'u': {
            unsigned long value = longarg ?
                va_arg(ap, unsigned long) :
                va_arg(ap, unsigned int);

            buf_put_unsigned(out, n, &pos,
                             value, 10, 0);
            break;
        }

        /* %x / %lx：十六进制整数 */
        case 'x': {
            unsigned long value = longarg ?
                va_arg(ap, unsigned long) :
                va_arg(ap, unsigned int);

            unsigned width = longarg ?
                2 * sizeof(unsigned long) :
                2 * sizeof(unsigned int);

            buf_put_unsigned(out, n, &pos,
                             value, 16, width);
            break;
        }

        /* %p：指针地址 */
        case 'p': {
            void *ptr = va_arg(ap, void *);

            buf_putc(out, n, &pos, '0');
            buf_putc(out, n, &pos, 'x');

            buf_put_unsigned(
                out, n, &pos,
                (unsigned long)(uintptr_t)ptr,
                16,
                2 * sizeof(uintptr_t)
            );
            break;
        }

        /* 不支持的格式，保留原始字符 */
        default:
            buf_putc(out, n, &pos, '%');

            if (longarg) {
                buf_putc(out, n, &pos, 'l');
            }

            buf_putc(out, n, &pos, *fmt);
            break;
        }

        ++fmt;
    }

    /* 添加字符串结束符 */
    if (out && n > 0) {
        out[pos < n ? pos : n - 1] = '\0';
    }

    if (pos > (size_t)INT_MAX) {
        return -1;
    }

    return (int)pos;
}