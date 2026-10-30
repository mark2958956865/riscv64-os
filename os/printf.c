// #include "os.h"
// #include <stdarg.h>
// #include <stddef.h>

// void uart_puts(char *s)
// {
//     while (*s) {
//         sbi_console_putchar(*s++);
//     }
// }

// static int _vsnprintf(char *out, size_t n, const char *s, va_list vl)
// {
//     int format = 0;
//     int longarg = 0;
//     size_t pos = 0;
//     for (; *s; s++) {
//         if (format) {
//             switch(*s) {
//                 case 'l': {
//                     longarg = 1;
//                     break;
//                 }
//                 case 'p': {
//                     longarg = 1;
//                     if (out && pos < n) {
//                         out[pos] = '0';
//                     }
//                     pos++;
//                     if (out && pos < n) {
//                         out[pos] = 'x';
//                     }
//                     pos++;
//                     break;
//                 }
//                 case 'x': {
//                     long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
//                     int hexdigits = 2*(longarg ? sizeof(long) : sizeof(int))-1;
//                     for(int i = hexdigits; i >= 0; i--) {
//                         int d = (num >> (4*i)) & 0xF;
//                         if (out && pos < n) {
//                             out[pos] = (d < 10 ? '0' + d : 'a' + d - 10);
//                         }
//                         pos++;
//                     }
//                     longarg = 0;
//                     format = 0;
//                     break;
//                 }
//                 case 'd': {
//                     long num = longarg ? va_arg(vl, long) : va_arg(vl, int);
//                     if (num < 0) {
//                         num = -num;
//                         if (out && pos < n) {
//                             out[pos] = '-';
//                         }
//                         pos++;
//                     }
//                     long digits = 1;
//                     for (long nn = num; nn /= 10; digits++);
//                     for (int i = digits-1; i >= 0; i--) {
//                         if (out && pos + i < n) {
//                             out[pos + i] = '0' + (num % 10);
//                         }
//                         num = num / 10;
//                     }
//                     pos += digits;
//                     longarg = 0;
//                     format = 0;
//                     break;
//                 }
//                 case 's': {
//                     const char *s2 = va_arg(vl, const char*);
//                     while (*s2) {
//                         if (out && pos < n) {
//                             out[pos] = *s2;
//                         }
//                         pos++;
//                         s2++;
//                     }
//                     longarg = 0;
//                     format = 0;
//                     break;
//                 }
//                 case 'c': {
//                     if (out && pos < n) {
//                         out[pos] = (char)va_arg(vl, int);
//                     }
//                     pos++;
//                     longarg = 0;
//                     format = 0;
//                     break;
//                 }
//                 default:
//                     break;
//             }
//         } else if (*s == '%') {
//             format = 1;
//         } else {
//             if (out && pos < n) {
//                 out[pos] = *s;
//             }
//             pos++;
//         }
//     }

//     if (out && pos < n) {
//         out[pos] = 0;
//     } else if (out && n) {
//         out[n-1] = 0;
//     }
//     return pos;
// }

// static char out_buf[1000];
// static int _vprintf(const char* s, va_list vl)
// {
//     int res = _vsnprintf(NULL, -1, s, vl);
//     if (res + 1 >= sizeof(out_buf)) {
//         uart_puts("error: output string size overflow\n");
//         while(1) {}
//     }
//     _vsnprintf(out_buf, res + 1, s, vl);
//     uart_puts(out_buf);
//     return res;
// }

// int printf(const char* s, ...)
// {
//     int res = 0;
//     va_list vl;
//     va_start(vl, s);
//     res = _vprintf(s, vl);
//     va_end(vl);
//     return res;
// }

// void panic(const char *s)
// {
//     printf("panic: ");
//     printf(s);
//     printf("\n");
//     while(1){}
// }



#include "os.h"
#include <stdint.h>
#include <limits.h>

#define PRINTF_BUF_SIZE 1000

static char out_buf[PRINTF_BUF_SIZE];

void uart_puts(const char *s)
{
    if (!s) {
        return;
    }

    while (*s) {
        sbi_console_putchar((unsigned char)*s++);
    }
}

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

static int _vsnprintf(char *out, size_t n,
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

/* 管理缓冲区并输出格式化结果 */
static int _vprintf(const char *fmt, va_list ap)
{
    va_list copy;

    /* 第一次遍历：计算输出长度 */
    va_copy(copy, ap);

    int needed = _vsnprintf(NULL, 0, fmt, copy);

    va_end(copy);

    /* 检查缓冲区空间 */
    if (needed < 0 ||
        (size_t)needed >= sizeof(out_buf)) {

        uart_puts("printf: output buffer overflow\n");
        return -1;
    }

    /* 第二次遍历：真正生成字符串 */
    va_copy(copy, ap);

    int written = _vsnprintf(
        out_buf,
        sizeof(out_buf),
        fmt,
        copy
    );

    va_end(copy);

    if (written < 0) {
        return -1;
    }

    /* 通过 OpenSBI 逐字符输出 */
    for (int i = 0; i < written; ++i) {
        sbi_console_putchar(
            (unsigned char)out_buf[i]
        );
    }

    return written;
}

int printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    int written = _vprintf(fmt, ap);

    va_end(ap);

    return written;
}

/* 内核严重错误处理 */
void panic(const char *msg)
{
    printf("panic: %s\n", msg);

    while (1) {
    }
}

