#include <timeros/address.h>
#include "os.h"
#include <stdint.h>
#include <timeros/assert.h>
#include "riscv.h"

/* 给定一个u64 转换为PhysAddr */
PhysAddr phys_addr_from_size_t(uint64_t v) {
    PhysAddr addr;
    addr.value = v & ((1ULL << PA_WIDTH_SV39) - 1);
    return addr;
}

/*给定一个u64 转换为PhysPageNum */
PhysPageNum phys_page_num_from_size_t(uint64_t v) {
    PhysPageNum pageNum;
    pageNum.value = v & ((1ULL << PPN_WIDTH_SV39) - 1);
    return pageNum;
}

/* 给定一个PhysAddr转换为u64 */
uint64_t size_t_from_phys_addr(PhysAddr v) {
    return v.value;
}

/* 给定一个PhysPageNum 转换为u64 */
uint64_t size_t_from_phys_page_num(PhysPageNum v) {
    return v.value;
}

/* 从物理页号转换为实际物理地址 */
PhysAddr phys_addr_from_phys_page_num(PhysPageNum ppn)
{
    PhysAddr addr;
    addr.value = ppn.value << PAGE_SIZE_BITS ;
    return addr;
}

/* 给定一个u64 转换为VirtAddr */
VirtAddr virt_addr_from_size_t(uint64_t v) {
    VirtAddr addr;
    addr.value = v & ((1ULL << VA_WIDTH_SV39) - 1);
    return addr;
}

/* 给定一个u64 转换为VirtPageNum */
VirtPageNum virt_page_num_from_size_t(uint64_t v) {
    VirtPageNum pageNum;
    pageNum.value = v & ((1ULL << VPN_WIDTH_SV39) - 1);
    return pageNum;
}

/*给定一个VirtAddr 转换为一个u64 */
uint64_t size_t_from_virt_addr(VirtAddr v) {
    if (v.value >= (1ULL << (VA_WIDTH_SV39 - 1))) {
        return v.value | ~((1ULL << VA_WIDTH_SV39) - 1);
    } else {
        return v.value;
    }
}

/* 给定一个VirtPageNum 转换为 u64*/
uint64_t size_t_from_virt_page_num(VirtPageNum v) {
    return v.value;
}

/* 物理地址向下取整 */
PhysPageNum floor_phys(PhysAddr phys_addr) {
    PhysPageNum phys_page_num;
    phys_page_num.value = phys_addr.value / PAGE_SIZE;
    return phys_page_num;
}

/* 物理地址向上取整 */
PhysPageNum ceil_phys(PhysAddr phys_addr) {
    PhysPageNum phys_page_num;
    phys_page_num.value = (phys_addr.value + PAGE_SIZE - 1) / PAGE_SIZE;
    return phys_page_num;
}

/* 虚拟地址向下取整，得到虚拟页号 */
VirtPageNum floor_virts(VirtAddr virt_addr)
{
    VirtPageNum virt_page_num;
    virt_page_num.value = virt_addr.value / PAGE_SIZE;
    return virt_page_num;
}

/* 把虚拟地址转换为虚拟页号 */
VirtPageNum virt_page_num_from_virt_addr(VirtAddr virt_addr)
{
    VirtPageNum vpn;
    vpn.value = virt_addr.value / PAGE_SIZE;
    return vpn;
}

/* 初始化物理页分配器 */
void StackFrameAllocator_new(StackFrameAllocator *allocator)
{
    allocator->start = 0;
    allocator->current = 0;
    allocator->end = 0;

    initStack(&allocator->recycled);
}

/* 设置可管理的物理页号区间 [l, r) */
void StackFrameAllocator_init(
    StackFrameAllocator *allocator,
    PhysPageNum l,
    PhysPageNum r)
{
    if (l.value >= r.value) {
        printk("Invalid physical page range!\n");
        return;
    }

    allocator->start = l.value;
    allocator->current = l.value;
    allocator->end = r.value;

    // 初始化时清空回收栈
    initStack(&allocator->recycled);
}

/* 分配一个 4 KiB 物理页 */
PhysPageNum StackFrameAllocator_alloc(
    StackFrameAllocator *allocator)
{
    PhysPageNum ppn = {0};

    // 优先使用已经回收的物理页
    if (!isEmpty(&allocator->recycled)) {
        ppn.value = pop(&allocator->recycled);
    }
    else {
        // 检查是否还有未分配的物理页
        if (allocator->current >= allocator->end) {
            return ppn;
        }
        // 获取一个新的物理页号
        ppn.value = allocator->current++;
    }
    // 分配失败，不允许继续清零内存
    if (ppn.value == 0) {
        return ppn;
    }
    // 将物理页号转换为物理地址
    PhysAddr addr = phys_addr_from_phys_page_num(ppn);
    // 清空整个物理页
    memset((void *)(uintptr_t)addr.value, 0, PAGE_SIZE);

    return ppn;
}

/* 回收一个物理页 */
void StackFrameAllocator_dealloc(
    StackFrameAllocator *allocator,
    PhysPageNum ppn)
{
    uint64_t ppnValue = ppn.value;
    // 检查物理页是否属于已分配区域
    if (ppnValue < allocator->start ||
        ppnValue >= allocator->current) {
        printk("Invalid physical page: %lx\n",
               (unsigned long)ppnValue);
        return;
    }
    // 检查物理页是否已经回收，防止重复释放
    for (int i = 0; i <= allocator->recycled.top; i++) {
        if (allocator->recycled.data[i] == ppnValue) {
            printk("Physical page already recycled!\n");
            return;
        }
    }
    // 回收栈已满，不能继续压栈
    if (isFull(&allocator->recycled)) {
        printk("Physical page recycle stack overflow!\n");
        return;
    }

    // 将物理页号放入回收栈
    push(&allocator->recycled, ppnValue);
}

static StackFrameAllocator FrameAllocatorImpl;
extern char kernelend[];

void frame_allocator_init(void)
{
    uintptr_t start = (uintptr_t)kernelend;
    // 避开 Bootloader 加载的 OS 区域
    if (start < MEMORY_START) {
        start = MEMORY_START;
    }
    // 确认仍有可用内存
    assert(start < MEMORY_END);
    // 初始化物理页分配器
    StackFrameAllocator_new(&FrameAllocatorImpl);
    StackFrameAllocator_init(
        &FrameAllocatorImpl,
        ceil_phys(phys_addr_from_size_t(start)),
        floor_phys(phys_addr_from_size_t(MEMORY_END))
    );
    printk("Memory start: %lx\n", (unsigned long)start);
    printk("Memory end: %lx\n", (unsigned long)MEMORY_END);
}

// void frame_allocator_test(void)
// {
//     StackFrameAllocator_new(&FrameAllocatorImpl);
//     // 物理页分配必须避开内核结束地址
//     reg_t start = (reg_t)end;
//     if (start < MEMORY_START) {
//         start = MEMORY_START;
//     }
//     if (start >= MEMORY_END) {
//         panic("No available physical memory!");
//     }
//     StackFrameAllocator_init(
//         &FrameAllocatorImpl,
//         ceil_phys(phys_addr_from_size_t(start)),
//         floor_phys(phys_addr_from_size_t(MEMORY_END))
//     );
//     PhysPageNum frame[5];
//     PhysPageNum frame_test[5];
//     // 第一次分配
//     for (int i = 0; i < 5; i++) {
//         frame[i] =
//             StackFrameAllocator_alloc(&FrameAllocatorImpl);
//         if (frame[i].value == 0) {
//             panic("Allocation failed!");
//         }
//         printk("alloc: %lx\n",
//                (unsigned long)frame[i].value);
//     }
//     // 回收物理页
//     for (int i = 0; i < 5; i++) {
//         StackFrameAllocator_dealloc(
//             &FrameAllocatorImpl, frame[i]
//         );
//     }
//     // 再次分配
//     for (int i = 0; i < 5; i++) {
//         frame_test[i] =
//             StackFrameAllocator_alloc(&FrameAllocatorImpl);

//         if (frame_test[i].value == 0) {
//             panic("Reallocation failed!");
//         }
//         printk("realloc: %lx\n",
//                (unsigned long)frame_test[i].value);
//     }
// }

/* 新建一个页表项 */
PageTableEntry PageTableEntry_new(PhysPageNum ppn, uint8_t PTEFlags) {
    PageTableEntry entry;
    entry.bits = ((ppn.value & ((1ULL << 44) - 1)) << 10) | PTEFlags;
    return entry;
}

/* 创建空页表项 */
PageTableEntry PageTableEntry_empty() {
    PageTableEntry entry;
    entry.bits = 0;
    return entry;
}

/* 获取下级页表的物理页号 */
PhysPageNum PageTableEntry_ppn(PageTableEntry *entry) {
    PhysPageNum ppn;
    ppn.value = (entry->bits >> 10) & ((1ULL << 44) - 1);
    return ppn;
}

/* 获取页表项的标志位 */
uint8_t PageTableEntry_flags(PageTableEntry *entry) {
    return entry->bits & 0xFF;
}

/* 判断页表项是否为空 */
bool PageTableEntry_is_valid(PageTableEntry *entry) {
    uint8_t entryFlags = PageTableEntry_flags(entry);
    return (entryFlags & PTE_V) != 0;
}

uint8_t* get_bytes_array(PhysPageNum ppn)
{
    // 先从物理页号转换为物理地址
    PhysAddr addr = phys_addr_from_phys_page_num(ppn);
    return (uint8_t *)(uintptr_t)addr.value;
}

PageTableEntry* get_pte_array(PhysPageNum ppn)
{
    // 先从物理页号转换为物理地址
    PhysAddr addr = phys_addr_from_phys_page_num(ppn);
    return (PageTableEntry *)(uintptr_t)addr.value;
}

void indexes(VirtPageNum vpn, size_t *result)
{
    size_t idx[3];
    for (int i = 2; i >= 0; i--) {
        idx[i] = vpn.value & 0x1FF;
        vpn.value >>= 9;
    }
    for (int i = 0; i < 3; i++) {
        result[i] = idx[i];
    }
}

/* 查找虚拟页对应的第三级页表项，
 * 如果中间页表不存在，则自动分配物理页创建。
 */
PageTableEntry *find_pte_create(PageTable *pt, VirtPageNum vpn)
{
    // 1. 获取虚拟页号的三级索引
    size_t idx[3];
    indexes(vpn, idx);
    // 2. 从根页表开始遍历
    PhysPageNum ppn = pt->root_ppn;
    if (ppn.value == 0) {
        return NULL;
    }
    // 3. 依次查找三级页表
    for (int i = 0; i < 3; i++)
    {
        // 获取当前页表中对应的页表项
        PageTableEntry *pte = &get_pte_array(ppn)[idx[i]];
        // 第三级：直接返回最终页表项
        if (i == 2) {
            return pte;
        }
        // 4. 如果中间页表项无效，需要创建下一级页表
        if (!PageTableEntry_is_valid(pte))
        {
            // 通过物理页分配器申请一页内存
            PhysPageNum frame =
                StackFrameAllocator_alloc(&FrameAllocatorImpl);
            // 分配失败
            if (frame.value == 0) {
                return NULL;
            }
            // 创建页表项，指向新分配的下一级页表
            *pte = PageTableEntry_new(frame, PTE_V);
        }
        // 中间页表项不能是叶子页表项
        // R/W/X 均为 0，才表示指向下一级页表
        if (PageTableEntry_flags(pte) &
            (PTE_R | PTE_W | PTE_X)) {
            return NULL;
        }
        // 5. 获取下一级页表的物理页号
        ppn = PageTableEntry_ppn(pte);
    }
    return NULL;
}

/*
 * 查找虚拟页号 vpn 对应的第三级页表项
 * 与 find_pte_create() 的区别：
 * 只查找，不创建新的页表。
 * 返回值：
 * 成功：返回第三级页表项的指针
 * 失败：返回 NULL
 */
PageTableEntry *find_pte(PageTable *pt, VirtPageNum vpn)
{
    // 1. 获取虚拟页号对应的三级索引
    size_t idx[3];
    indexes(vpn, idx);
    // 2. 从根页表开始查找
    PhysPageNum ppn = pt->root_ppn;
    if (ppn.value == 0) {
        return NULL;
    }
    // 3. 逐级遍历三级页表
    for (int i = 0; i < 3; i++)
    {
        // 获取当前页表中对应页表项的地址
        PageTableEntry *pte = &get_pte_array(ppn)[idx[i]];
        // 页表项无效，说明对应映射不存在
        if (!PageTableEntry_is_valid(pte)) {
            return NULL;
        }
        // 已经找到第三级页表项
        if (i == 2) {
            return pte;
        }
        // 中间页表项不能是叶子页表项
        if (PageTableEntry_flags(pte) &
            (PTE_R | PTE_W | PTE_X)) {
            return NULL;
        }
        // 取出下一级页表的物理页号
        ppn = PageTableEntry_ppn(pte);
    }
    return NULL;
}

/*连续虚拟地址映射
 * pt       : 页表
 * va       : 起始虚拟地址
 * pa       : 起始物理地址
 * size     : 映射长度，单位为字节
 * pteflags : 页表项权限
 当前要求 va 和 pa 均按 4 KiB 对齐。*/
void PageTable_map(PageTable *pt, VirtAddr va,
                   PhysAddr pa, uint64_t size,
                   uint8_t pteflags)
{
    // 1. 检查参数
    assert(pt != NULL);
    assert(size > 0);
    assert((va.value & (PAGE_SIZE - 1)) == 0);
    assert((pa.value & (PAGE_SIZE - 1)) == 0);
    // 防止计算结束地址时溢出
    assert(size - 1 <= UINT64_MAX - va.value);
    // 2. 将起始地址转换成页号
    VirtPageNum vpn = floor_virts(va);
    PhysPageNum ppn = floor_phys(pa);
    // 3. 计算最后一个需要映射的虚拟页号
    uint64_t last = (va.value + size - 1) / PAGE_SIZE;
    // 4. 逐页建立映射
    for (;;)
    {
        // 查找页表项，必要时创建中间页表
        PageTableEntry *pte = find_pte_create(pt, vpn);
        // 检查页表项是否成功找到
        assert(pte != NULL);
        // 防止重复映射
        assert(!PageTableEntry_is_valid(pte));
        // 建立当前页的映射关系
        *pte = PageTableEntry_new(ppn, PTE_V | pteflags);
        // 已经映射到最后一页
        if (vpn.value == last) {
            break;
        }
        // 虚拟页号和物理页号同时增加
        vpn.value++;
        ppn.value++;
    }
}

/* 解除一个虚拟页的映射 */
void PageTable_unmap(PageTable *pt, VirtPageNum vpn)
{
    // 查找已有的有效页表项
    PageTableEntry *pte = find_pte(pt, vpn);
    // 检查是否查找成功
    assert(pte != NULL);
    // 确认该页表项有效
    assert(PageTableEntry_is_valid(pte));
    // 清空页表项
    *pte = PageTableEntry_empty();
}

/* 内核代码段结束地址，由链接脚本定义 */
extern char etext[];
/* 内核页表 */
static PageTable kernel_pagetable;
/* 创建内核页表 */
PageTable kvmmake(void)
{
    PageTable pt;
    // 1. 分配一页物理内存作为根页表
    PhysPageNum root_ppn =
        StackFrameAllocator_alloc(&FrameAllocatorImpl);
    // 检查分配是否成功
    assert(root_ppn.value != 0);
    pt.root_ppn = root_ppn;
    uintptr_t text_end = (uintptr_t)etext;
    assert(text_end > KERNBASE);
    assert(text_end < MEMORY_END);
    printk("root_ppn: %lx\n",(unsigned long)root_ppn.value);

    // 2. 映射内核代码段：可读、可执行
    PageTable_map(
        &pt,
        virt_addr_from_size_t(KERNBASE),
        phys_addr_from_size_t(KERNBASE),
        text_end - KERNBASE,
        PTE_R | PTE_X
    );
    printk("Finish kernel text map!\n");
    // 3. 映射内核数据段和后续 RAM：可读、可写
    PageTable_map(
        &pt,
        virt_addr_from_size_t(text_end),
        phys_addr_from_size_t(text_end),
        MEMORY_END - text_end,
        PTE_R | PTE_W
    );
    printk("Finish kernel data and RAM map!\n");
    return pt;
}

/* 初始化内核页表 */
void kvminit(void)
{
    kernel_pagetable = kvmmake();
}

/* 为当前 Hart 开启 Sv39 分页 */
void kvminithart(void)
{
    // 1. 同步之前写入的页表内容
    sfence_vma();

    // 2. 将根页表物理页号写入 satp
    w_satp(MAKE_SATP(kernel_pagetable.root_ppn.value));

    // 3. 同步新的地址转换配置
    sfence_vma();

    // 4. 读取 satp，验证配置
    reg_t satp = r_satp();

    printk("satp: %lx\n", (unsigned long)satp);
}
