#include "os.h"
#include "task.h"
#include "address.h"

// void os_main(void)
// {
//     // printk("Physical Memory Test!\n");

//     // frame_allocator_test();

//     // printk("Frame allocator test finished!\n");

//     // while (1) {
//     // }
//     printk("Hello Quard Star OS!\n");

//     frame_allocator_init();

//     trap_init();

//     task_init();
    
//     timer_init();

//     run_first_task();
// }

void os_main(void)
{
    printk("Hello Quard Star OS!\n");

    // 1. 初始化异常处理
    trap_init();

    // 2. 初始化物理页分配器
    frame_allocator_init();

    // 3. 创建内核页表并建立恒等映射
    kvminit();

    // 4. 正式启用 Sv39
    kvminithart();

    printk("Sv39 enabled successfully!\n");

    // 5. 暂时停在内核态测试
    while (1) {
    }

    // 下面暂时不运行
    // task_init();
    // timer_init();
    // run_first_task();
}