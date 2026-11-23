#include "os.h"
#include "types.h"
#include "task.h"
#include "address.h"
#include "loader.h"

extern int sbi_console_getchar(void);
static void __sys_yield(void);

static int translate_user_addr(PageTable *pt, reg_t user_va, uint64_t *pa_out, reg_t required_perm)
{
    // 1. 虚拟地址转换为虚拟页号
    VirtPageNum vpn = floor_virts(virt_addr_from_size_t(user_va));
    // 2. 使用已有函数查询三级页表
    PageTableEntry *pte = find_pte(pt, vpn);
    if (pte == NULL) {
        return -1;
    }
    // 3. 检查页表项有效性
    if (!PageTableEntry_is_valid(pte)) {
        return -1;
    }
    // 4. 检查用户读取权限
    reg_t flags = PageTableEntry_flags(pte);
    reg_t required_flags = PTE_U | required_perm;
    if ((flags & required_flags) != required_flags) {
        return -1;
    }
    // RISC-V 不允许只写不可读的页表项
    if ((flags & PTE_W) && !(flags & PTE_R)) {
        return -1;
    }
    // 5. 提取物理页号
    // RISC-V PTE 的 PPN 位于 bits[53:10]
    uint64_t ppn = (pte->bits >> 10) & ((1ULL << 44) - 1);
    // 6. 物理页号转换为物理页起始地址
    uint64_t page_pa = ppn << 12;
    // 7. 检查物理页是否属于内核恒等映射的 RAM
    if (page_pa < KERNBASE || page_pa >= MEMORY_END || MEMORY_END - page_pa < PAGE_SIZE) {
        return -1;
    }
    // 8. 加上用户虚拟地址的页内偏移
    uint64_t offset = user_va & (PAGE_SIZE - 1);
    *pa_out = page_pa + offset;
    return 0;
}

static reg_t __sys_write(reg_t fd, reg_t buf, reg_t len)
{
    // 1. 仅支持标准输出
    if (fd != 1) {
        return (reg_t)-1;
    }

    if (len == 0) {
        return 0;
    }
    // 2. 检查用户地址范围及地址溢出
    if (buf >= TRAPFRAME || len > TRAPFRAME - buf) {
        return (reg_t)-1;
    }
    // 3. 获取当前任务用户页表的 satp token
    reg_t user_satp = current_user_token();
    // 确保当前采用 Sv39
    if ((user_satp >> 60) != 8) {
        return (reg_t)-1;
    }
    // 4. 从 satp 中提取根页表物理页号
    PageTable pt;
    pt.root_ppn.value = user_satp & ((1ULL << 44) - 1);
    reg_t written = 0;
    // 5. 逐页访问用户缓冲区
    while (written < len)
    {
        reg_t user_va = buf + written;
        uint64_t paddr;
        // 用户虚拟地址转换为物理地址
        if (translate_user_addr(
                &pt, user_va, &paddr, PTE_R) != 0) {
            return (reg_t)-1;
        }
        // 6. 计算当前页还剩多少字节
        reg_t page_offset = user_va & (PAGE_SIZE - 1);
        reg_t remain_in_page = PAGE_SIZE - page_offset;
        reg_t remain_total = len - written;
        reg_t copy_len = remain_total < remain_in_page ? remain_total : remain_in_page;
        // 7. 内核 RAM 恒等映射：
        //    此物理地址可作为内核虚拟地址访问
        const unsigned char *data = (const unsigned char *)(uintptr_t)paddr;
        // 8. 按 len 输出，不依赖字符串结束符
        for (reg_t i = 0; i < copy_len; i++)
        {
            printk("%c", (int)data[i]);
        }
        written += copy_len;
    }
    // 返回实际写入的字节数
    return written;
}

static reg_t __sys_read(reg_t fd, reg_t buf, reg_t len)
{
    // 1. 仅支持标准输入
    if (fd != 0) {
        return (reg_t)-1;
    }
    if (len == 0) {
        return 0;
    }
    // 当前阶段只支持读取一个字符
    if (len != 1) {
        return (reg_t)-1;
    }
    // 2. 检查用户缓冲区地址
    if (buf >= TRAPFRAME || len > TRAPFRAME - buf) {
        return (reg_t)-1;
    }
    // 3. 获取当前任务的用户页表
    reg_t user_satp = current_user_token();
    if ((user_satp >> 60) != 8) {
        return (reg_t)-1;
    }
    PageTable pt;
    pt.root_ppn.value = user_satp & ((1ULL << 44) - 1);
    // 4. 转换用户缓冲区地址
    uint64_t paddr;
    // read 需要向用户内存写入数据
    if (translate_user_addr(&pt, buf, &paddr, PTE_W) != 0) {
        return (reg_t)-1;
    }
    // 5. 循环等待串口输入
    int c;
    while (1)
    {
        c = sbi_console_getchar();
        if (c >= 0 && c <= 255) {
            break;
        }
        if (c != -1) {
            return (reg_t)-1;
        }
        // 没有输入，让出 CPU 执行其他任务
        schedule();
    }
    // 6. 将字符写入用户缓冲区
    unsigned char *data = (unsigned char *)(uintptr_t)paddr;
    data[0] = (unsigned char)c;
    // 7. 返回实际读取的字节数
    return 1;
}

static reg_t __sys_gettime(void)
{
    return (reg_t)get_time_us();
}

static void __sys_yield(void)
{
    schedule();
}

#define EXEC_NAME_MAX 64

/* 从当前进程的用户地址空间复制字符串
 *
 * 成功返回 0，失败返回 -1 */
static int copy_user_string(reg_t user_va, char *dst, size_t dst_size)
{
    if (dst == NULL || dst_size == 0 || user_va == 0) {
        return -1;
    }
    TaskControlBlock *proc = current_proc();
    for (size_t i = 0; i < dst_size; i++)
    {
        // 防止地址越界及整数溢出
        if (user_va >= TRAPFRAME || i >= TRAPFRAME - user_va) {
            return -1;
        }
        // 用户虚拟地址转换为物理地址
        uint64_t pa;
        if (translate_user_addr(&proc->pagetable, user_va + i, &pa, PTE_R) != 0) {
            return -1;
        }
        // 当前内核对 RAM 使用恒等映射
        char c = *(const char *)(uintptr_t)pa;
        dst[i] = c;
        // 找到字符串结束符
        if (c == '\0') {
            return 0;
        }
    }
    // 超过最大长度仍未结束
    dst[dst_size - 1] = '\0';
    return -1;
}


/*
 * exec 系统调用
 * name_va：用户态程序名称字符串的虚拟地址
 */
static reg_t __sys_exec(reg_t name_va)
{
    char name[EXEC_NAME_MAX];
    // 1. 将用户字符串复制到内核缓冲区
    if (copy_user_string(name_va, name, sizeof(name)) != 0)
    {
        return (reg_t)-1;
    }
    // 2. 执行指定应用程序
    return (reg_t)exec(name);
}


/*
 * 内核态 exit 系统调用处理函数
 */
static void __sys_exit(reg_t exit_code)
{
    exit_current_and_run_next((int)exit_code);
}

static reg_t __sys_waitpid(reg_t pid)
{
    return (reg_t)waitpid((int)pid);
}

reg_t __SYSCALL(reg_t id, reg_t arg1,reg_t arg2, reg_t arg3)
{
    switch (id) {
        case __NR_write:
            return __sys_write(arg1, arg2, arg3);
        case __NR_read:
            return __sys_read(arg1, arg2, arg3);
        case __NR_sched_yield:
            __sys_yield();
            return 0;
        case __NR_gettimeofday:
            return __sys_gettime();
        case __NR_clone:
            return (reg_t)__sys_fork();
        case __NR_execve:
            return __sys_exec(arg1);
        case __NR_exit:
            __sys_exit(arg1);
            return 0;
        case __NR_waitid:
            return __sys_waitpid(arg1);
        default:
            printk("Unsupported syscall id: %lu\n",
                   (unsigned long)id);
            return (reg_t)-1;
    }
}
