#include "os.h"
#include "task.h"
#include "loader.h"
#include "address.h"


void os_main(void)
{
    printk("Hello Quard Star OS!\n");

    // 1. 初始化物理内存分配器
    frame_allocator_init();

    // 2. 创建内核页表
    kvminit();

    // 3. 初始化进程控制块数组（新增）
    procinit();

    // 4. 启用内核虚拟地址空间
    kvminithart();

    // 5. 初始化 Trap
    trap_init();

    // 6. 获取应用程序信息
    size_t num_app = get_num_app();
    printk("num app: %lu\n",
           (unsigned long)num_app);

    if (num_app == 0)
    {
        panic("No user applications");
    }

    get_app_names();

    // // 7. 加载并初始化用户进程
    // for (size_t i = 0; i < num_app; i++)
    // {
    //     load_app(i);
    //     app_init(i);
    // }

    // 7. 只加载第一个初始进程
    // 必须确保 app0 是 initproc
    load_app(0);
    app_init(0);

    printk("User applications initialized!\n");

    // 8. 初始化时钟
    timer_init();

    // 9. 启动第一个任务
    run_first_task();
}


