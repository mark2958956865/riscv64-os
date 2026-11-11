#include <timeros/assert.h>
#include <timeros/stdio.h>

/* 断言失败：输出错误信息并停止执行 */
void assertion_failure(const char *exp,
                       const char *file,
                       int line)
{
    printk("\nAssertion failed!\n");
    printk("Expression: %s\n", exp);
    printk("File: %s\n", file);
    printk("Line: %d\n", line);

    while (1) {
        // 停止执行
    }
}