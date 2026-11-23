#include <timeros/loader.h>
#include <timeros/assert.h>
#include <timeros/address.h>
#include <timeros/task.h>

// 汇编文件 link_app.S 中定义的全局符号
extern uint64_t _num_app[];

// 获取应用程序数量
size_t get_num_app(void)
{
    return (size_t)_num_app[0];
}

// 获取指定应用程序的起始地址和大小
AppMetadata get_app_data(size_t app_id)
{
    AppMetadata metadata;

    size_t num_app = get_num_app();

    // 先检查编号是否合法
    assert(app_id < num_app);

    // 跳过 _num_app[0]，读取应用程序起始地址
    metadata.start = _num_app[app_id + 1];

    // 使用相邻地址计算对应的数据范围
    metadata.size = _num_app[app_id + 2]
                  - _num_app[app_id + 1];

    return metadata;
}

static uint8_t flags_to_mmap_prot(uint8_t flags)
{
    return (flags & PF_R ? PTE_R : 0) |
           (flags & PF_W ? PTE_W : 0) |
           (flags & PF_X ? PTE_X : 0);
}

/*
 * 检查 ELF 文件格式和 Program Header 边界
 * 返回 0 成功，-1 失败
 */
static int elf_check(const AppMetadata *meta)
{
    if (meta->size < sizeof(elf64_ehdr_t)) {
        return -1;
    }
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)meta->start;
    const uint8_t *ident = ehdr->e_ident;
    // ELF 魔数：0x7F 'E' 'L' 'F'
    if (ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F') {
        return -1;
    }
    // 检查是否为 RISC-V64
    if (ehdr->e_machine != EM_RISCV || ident[EI_CLASS] != ELFCLASS64) {
        return -1;
    }
    // 检查程序头大小与范围
    if (ehdr->e_phentsize != sizeof(elf64_phdr_t) ||
        ehdr->e_phoff > meta->size ||
        ehdr->e_phnum >(meta->size - ehdr->e_phoff) / ehdr->e_phentsize) {
        return -1;
    }
    return 0;
}

/*
 * 加载 ELF 的 PT_LOAD 段
 * 分配用户物理页、复制文件数据、清零 BSS
 */
static int load_segment(
    const AppMetadata *meta,
    TaskControlBlock *proc)
{
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)meta->start;
    const uint8_t *image = (const uint8_t *)meta->start;
    proc->ustack = 0;
    for (size_t i = 0; i < ehdr->e_phnum; i++)
    {
        const elf64_phdr_t *phdr = (const elf64_phdr_t *)(image + ehdr->e_phoff + i * ehdr->e_phentsize);
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        // 校验文件大小和内存大小
        if (phdr->p_filesz > phdr->p_memsz || phdr->p_offset > meta->size ||
            phdr->p_filesz > meta->size - phdr->p_offset) {
            return -1;
        }
        uint64_t start_va = phdr->p_vaddr;
        if (phdr->p_memsz > TRAPFRAME || start_va >= TRAPFRAME ||
            start_va > TRAPFRAME - phdr->p_memsz) {
            return -1;
        }
        uint64_t end_va = start_va + phdr->p_memsz;
        if (end_va > proc->ustack) {
            proc->ustack = end_va;
        }
        if (phdr->p_memsz == 0) {
            continue;
        }
        // 按页对齐映射范围
        uint64_t map_start = start_va & ~(PAGE_SIZE - 1);
        uint64_t map_end = (end_va + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t perm = PTE_U | flags_to_mmap_prot(phdr->p_flags);
        for (uint64_t va = map_start;
             va < map_end;
             va += PAGE_SIZE)
        {
            // 当前版本要求不同 PT_LOAD 段不重叠映射
            VirtPageNum vpn = floor_virts(virt_addr_from_size_t(va));
            if (find_pte(&proc->pagetable, vpn) != NULL) {
                return -1;
            }
            // 分配物理页
            PhysPageNum ppn = kalloc();
            if (ppn.value == 0) {
                return -1;
            }
            PhysAddr pa = phys_addr_from_phys_page_num(ppn);
            uint8_t *dst = (uint8_t *)(uintptr_t)pa.value;
            // 整页清零，保证 BSS 初始化为 0
            memset(dst, 0, PAGE_SIZE);
            // 当前页与 ELF 文件数据的交集
            uint64_t copy_start = va > start_va ? va : start_va;
            uint64_t file_end = start_va + phdr->p_filesz;
            uint64_t page_end = va + PAGE_SIZE;
            uint64_t copy_end = page_end < file_end ? page_end : file_end;
            if (copy_end > copy_start)
            {
                uint64_t src_offset = phdr->p_offset + copy_start - start_va;
                memcpy(dst + copy_start - va, image + src_offset, copy_end - copy_start);
            }
            // 建立用户虚拟地址映射
            PageTable_map(
                &proc->pagetable,
                virt_addr_from_size_t(va),
                phys_addr_from_size_t(pa.value),
                PAGE_SIZE,
                perm
            );
        }
    }
    return 0;
}

/* 创建用户栈及 Guard Page */
static void proc_ustack(TaskControlBlock *p)
{
    // 留出 Guard Page 和一页用户栈
    p->ustack = 2 * PAGE_SIZE + PGROUNDUP(p->ustack);
    if (p->ustack >= TRAPFRAME) {
        panic("user stack address invalid");
    }
    // 记录用户地址空间上界
    p->base_size = p->ustack;
    PhysPageNum ppn = kalloc();
    if (ppn.value == 0) {
        panic("proc_ustack: kalloc failed");
    }
    PhysAddr pa = phys_addr_from_phys_page_num(ppn);
    // 初始化用户栈
    memset((void *)(uintptr_t)pa.value, 0, PAGE_SIZE);
    // 只映射用户栈，Guard Page 不映射
    PageTable_map(
        &p->pagetable,
        virt_addr_from_size_t(
            p->ustack - PAGE_SIZE),
        pa,
        PAGE_SIZE,
        PTE_R | PTE_W | PTE_U
    );
}

/* 加载指定编号的应用程序 */
void load_app(size_t app_id)
{
    // 注意：现有 get_app_data 使用零基索引
    AppMetadata metadata = get_app_data(app_id);
    if (elf_check(&metadata) != 0) {
        panic("Invalid RISC-V64 ELF");
    }
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)metadata.start;
    // 创建进程控制块与独立用户页表
    TaskControlBlock *proc = task_create_pt(app_id);
    if (proc == NULL) {
        panic("task_create_pt failed");
    }
    // 加载 ELF 程序段
    if (load_segment(&metadata, proc) != 0) {
        panic("load_segment failed");
    }
    // 保存程序入口地址
    proc->entry = (uint64_t)ehdr->e_entry;
    // 创建用户栈
    proc_ustack(proc);
}

// /* 加载指定编号的应用程序 */
// void load_app(size_t app_id)
// {
//     // 注意：现有 get_app_data 使用零基索引
//     AppMetadata metadata = get_app_data(app_id);

//     if (elf_check(&metadata) != 0) {
//         panic("Invalid RISC-V64 ELF");
//     }

//     const elf64_ehdr_t *ehdr =
//         (const elf64_ehdr_t *)metadata.start;

//     // 创建进程控制块与独立用户页表
//     TaskControlBlock *proc =
//         task_create_pt(app_id);

//     if (proc == NULL) {
//         panic("task_create_pt failed");
//     }

//     // 加载 ELF 程序段
//     if (load_segment(&metadata, proc) != 0) {
//         panic("load_segment failed");
//     }

//     // 保存程序入口地址
//     proc->entry = (uint64_t)ehdr->e_entry;

//     // 创建用户栈
//     proc_ustack(proc);
// }

// /* 解析并装载指定用户程序 */
// void load_app(size_t app_id)
// {
//     // 1. 获取应用程序 ELF 数据
//     AppMetadata metadata = get_app_data(app_id);
//     if (metadata.size < sizeof(elf64_ehdr_t)) {
//         panic("ELF file too small");
//     }
//     const uint8_t *elf =(const uint8_t *)(uintptr_t)metadata.start;
//     elf64_ehdr_t *ehdr =(elf64_ehdr_t *)(uintptr_t)metadata.start;
//     // 2. 检查 ELF 魔数
//     if (elf[0] != 0x7f || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F') {
//         panic("invalid ELF magic");
//     }
//     // 3. 检查是否为 RISC-V ELF64
//     if (ehdr->e_machine != EM_RISCV || ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
//         panic("only riscv64 ELF is supported");
//     }
//     // 4. 检查 Program Header 表是否有效
//     if (ehdr->e_phentsize != sizeof(elf64_phdr_t) || ehdr->e_phoff > metadata.size ||
//         ehdr->e_phnum > (metadata.size - ehdr->e_phoff) / sizeof(elf64_phdr_t)) {
//         panic("invalid ELF program headers");
//     }
//     // 5. 创建任务及独立用户页表
//     TaskControlBlock *proc = task_create_pt(app_id);
//     // 保存 ELF 程序入口地址
//     proc->entry = ehdr->e_entry;
//     // 记录所有程序段的最高结束地址
//     uint64_t max_end = 0;
//     size_t load_count = 0;
//     // 6. 遍历 Program Header
//     for (size_t i = 0; i < ehdr->e_phnum; i++)
//     {
//         const elf64_phdr_t *phdr =(const elf64_phdr_t *)(elf + ehdr->e_phoff + i * ehdr->e_phentsize);
//         // 只处理需要加载的程序段
//         if (phdr->p_type != PT_LOAD)
//             continue;
//         // 检查文件数据与内存大小
//         if (phdr->p_filesz > phdr->p_memsz ||
//             phdr->p_offset > metadata.size ||
//             phdr->p_filesz > metadata.size - phdr->p_offset) {
//             panic("invalid ELF segment");
//         }
//         if (phdr->p_memsz == 0)
//             continue;
//         uint64_t start_va = phdr->p_vaddr;
//         // 防止程序段越过用户地址空间上界
//         if (start_va >= TRAPFRAME || phdr->p_memsz > TRAPFRAME - start_va) {
//             panic("invalid segment virtual address");
//         }
//         uint64_t end_va = start_va + phdr->p_memsz;
//         uint64_t file_end = start_va + phdr->p_filesz;
//         if (end_va > max_end)
//             max_end = end_va;
//         load_count++;
//         // 7. 转换 ELF 权限为页表权限
//         uint8_t map_perm = PTE_U | flags_to_mmap_prot(phdr->p_flags);
//         if ((map_perm & PTE_W) && !(map_perm & PTE_R)) {
//             panic("invalid segment permissions");
//         }
//         // 8. 计算页对齐的映射范围
//         uint64_t map_start = start_va & ~((uint64_t)PAGE_SIZE - 1);
//         uint64_t map_end = PGROUNDUP(end_va);
//         // 9. 逐页分配、复制和映射
//         for (uint64_t va = map_start;va < map_end;va += PAGE_SIZE)
//         {
//             PhysPageNum ppn = kalloc();
//             if (ppn.value == 0) {
//                 panic("load_app: kalloc failed");
//             }
//             PhysAddr pa = phys_addr_from_phys_page_num(ppn);
//             // 清零物理页，保证 BSS 部分为 0
//             memset((void *)(uintptr_t)pa.value, 0, PAGE_SIZE);
//             // 计算当前页与 ELF 文件数据的交集
//             uint64_t copy_start = va > start_va ? va : start_va;
//             uint64_t page_end = va + PAGE_SIZE;
//             uint64_t copy_end = page_end < file_end ? page_end : file_end;
//             // 只复制 ELF 文件中实际存在的数据
//             if (copy_end > copy_start)
//             {
//                 uint64_t length = copy_end - copy_start;
//                 uint64_t src_offset = phdr->p_offset + (copy_start - start_va);
//                 uint64_t dst_offset = copy_start - va;
//                 memcpy((void *)(uintptr_t)(pa.value + dst_offset), elf + src_offset, (size_t)length);
//             }
//             // 建立用户虚拟页到物理页的映射
//             PageTable_map(&proc->pagetable, virt_addr_from_size_t(va), pa, PAGE_SIZE, map_perm);
//         }
//     }
//     if (load_count == 0) {
//         panic("ELF has no loadable segments");
//     }
//     // 10. 计算用户栈位置, 程序段后面留一页 Guard Page
//     uint64_t guard_start = PGROUNDUP(max_end);
//     if (guard_start >= TRAPFRAME ||
//         guard_start > TRAPFRAME - 2 * PAGE_SIZE) {
//         panic("no space for user stack");
//     }
//     proc->ustack = guard_start + 2 * PAGE_SIZE;
//     // 11. 分配用户栈物理页
//     PhysPageNum stack_ppn = kalloc();
//     if (stack_ppn.value == 0) {
//         panic("user stack allocation failed");
//     }
//     PhysAddr stack_pa = phys_addr_from_phys_page_num(stack_ppn);
//     // 12. 映射用户栈
//     PageTable_map(&proc->pagetable, virt_addr_from_size_t(proc->ustack - PAGE_SIZE), stack_pa, PAGE_SIZE, PTE_R | PTE_W | PTE_U);
//     // Guard Page 不进行映射

//     proc->base_size = proc->ustack;
// }

/* link_app.S 中定义的应用程序名称 */
extern char _app_names[];

/* 保存每个应用程序名称的指针 */
static const char *app_names[MAX_TASKS];

/* 获取所有应用程序的名称 */
void get_app_names(void)
{
    // 1. 获取应用程序数量
    size_t app_num = get_num_app();
    assert(app_num <= MAX_TASKS);
    // 2. 指向第一个应用程序名称
    const char *name = _app_names;
    printk("****** APPS ******\n");
    // 3. 依次读取每个应用程序名称
    for (size_t i = 0; i < app_num; i++)
    {
        // 保存当前应用程序名称的地址
        app_names[i] = name;
        // 打印应用编号及名称
        printk("app%lu: %s\n", (unsigned long)i, app_names[i]);
        // 移动到下一个应用程序名称
        name += strlen(name) + 1;
    }
    printk("******************\n");
}

/*
 * 根据应用程序名称获取 ELF 数据
 *
 * 成功：返回应用程序元数据
 * 失败：metadata.start 为 NULL
 */
AppMetadata get_app_data_by_name(const char *name)
{
    AppMetadata metadata = {0};
    if (name == NULL) {
        return metadata;
    }
    size_t app_num = get_num_app();
    for (size_t i = 0; i < app_num; i++)
    {
        if (app_names[i] != NULL && strcmp(name, app_names[i]) == 0)
        {
            return get_app_data(i);
        }
    }
    return metadata;
}

/* exec：替换当前进程的用户程序
 *
 * 成功返回 0，失败返回 -1
 * 保留原 PID、父进程关系、内核栈和 TaskContext */
int exec(const char *name)
{
    // 1. 根据程序名称查找 ELF 数据
    AppMetadata metadata = get_app_data_by_name(name);
    if (metadata.start == 0) {
        printk("exec: application not found: %s\n", name);
        return -1;
    }
    // 2. 校验 ELF 文件
    if (elf_check(&metadata) != 0) {
        printk("exec: invalid ELF\n");
        return -1;
    }
    const elf64_ehdr_t *ehdr = (const elf64_ehdr_t *)(uintptr_t)metadata.start;
    // 3. 获取当前进程
    TaskControlBlock *proc = current_proc();
    // 4. 暂存旧页表及地址空间大小
    PageTable old_pagetable = proc->pagetable;
    uint64_t old_size = proc->base_size;
    // 5. 构造临时进程映像
    // 不创建新进程，不分配新 PID
    TaskControlBlock new_image = {0};
    // 复用当前进程的 TrapContext 物理页
    new_image.trap_cx_ppn = proc->trap_cx_ppn;
    // 6. 创建新用户页表
    // 映射 Trampoline 和已有 TrapContext
    proc_pagetable(&new_image);
    // 7. 加载新 ELF 程序段
    if (load_segment(&metadata, &new_image) != 0)
    {
        // 加载失败，清理临时地址空间
        proc_freepagetable(&new_image.pagetable, new_image.ustack);
        return -1;
    }
    // 8. 创建新用户栈
    proc_ustack(&new_image);
    // 9. 新程序已经加载成功
    // 提交新的用户地址空间
    proc->pagetable = new_image.pagetable;
    proc->entry = (uint64_t)ehdr->e_entry;
    proc->ustack = new_image.ustack;
    proc->base_size = new_image.base_size;
    // 10. 获取当前进程的 TrapContext
    // TrapContext 物理页并没有重新分配
    TrapContext *cx = (TrapContext *)(uintptr_t)proc->trap_cx_ppn;
    // 11. 设置新程序入口和用户栈顶
    cx->sepc = proc->entry;
    cx->sp = proc->ustack;
    // 新程序启动时 a0 初始化为 0
    cx->a0 = 0;
    // kernel_satp、kernel_sp、trap_handler
    // 都属于当前进程，保持原值即可
    // 12. 释放旧用户地址空间
    // 注意：不能释放共用的 TrapContext 物理页
    proc_freepagetable(&old_pagetable, old_size);
    return 0;
}
