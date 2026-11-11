#ifndef TOS_ADDRESS_H
#define TOS_ADDRESS_H

#include <stdint.h>
#include <timeros/os.h>
#include <timeros/stack.h>
// #include <timeros/string.h>
// #include <timeros/assert.h>

#define PAGE_SIZE 0x1000        // 4kb  一页的大小
#define PAGE_SIZE_BITS 0xc      // 12   页内偏移地址长度

#define PA_WIDTH_SV39 56        //物理地址长度
#define VA_WIDTH_SV39 39        //虚拟地址长度
#define PPN_WIDTH_SV39 (PA_WIDTH_SV39 - PAGE_SIZE_BITS)  // 物理页号 44位 [55:12]
#define VPN_WIDTH_SV39 (VA_WIDTH_SV39 - PAGE_SIZE_BITS)   // 虚拟页号 27位 [38:12]

// #define MEMORY_END 0x80800000   // 0x80200000 ~ 0x80800000
// #define MEMORY_START 0x80400000

#define MEMORY_START 0x80600000UL
#define MEMORY_END   0x82000000UL
#define KERNBASE 0x80200000UL

#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)
#define PTE_G (1 << 5)
#define PTE_A (1 << 6)
#define PTE_D (1 << 7)

#define SATP_SV39 (8ULL << 60)

#define MAKE_SATP(ppn) (SATP_SV39 | ((uint64_t)(ppn) & ((1ULL << 44) - 1)))

/* 物理地址 */
typedef struct {
    uint64_t value;
} PhysAddr;

/* 虚拟地址 */
typedef struct {
    uint64_t value;
} VirtAddr;

/* 物理页号 */
typedef struct {
    uint64_t value;
} PhysPageNum;

/* 虚拟页号 */
typedef struct {
    uint64_t value;
} VirtPageNum;

/* 定义页表项 */
typedef struct
{
    uint64_t bits;
} PageTableEntry;

/* 栈式物理页分配器 */
typedef struct {
    uint64_t start;     // 可分配物理页号的起始位置
    uint64_t current;   // 下一个尚未分配的物理页号
    uint64_t end;       // 可分配物理页号的结束位置（不包含）
    Stack recycled;     // 已回收的物理页号栈
} StackFrameAllocator;

typedef struct {
    PhysPageNum root_ppn;
} PageTable;

/* 给定一个u64 转换为PhysAddr */
PhysAddr phys_addr_from_size_t(uint64_t v);

/* 给定一个u64 转换为PhysPageNum */
PhysPageNum phys_page_num_from_size_t(uint64_t v);

/* 给定一个PhysAddr转换为u64 */
uint64_t size_t_from_phys_addr(PhysAddr v);

/* 给定一个PhysPageNum 转换为u64 */
uint64_t size_t_from_phys_page_num(PhysPageNum v);

/* 从物理页号转换为实际物理地址 */
PhysAddr phys_addr_from_phys_page_num(PhysPageNum ppn);

/* 给定一个u64 转换为VirtAddr */
VirtAddr virt_addr_from_size_t(uint64_t v);

/* 给定一个u64 转换为VirtPageNum */
VirtPageNum virt_page_num_from_size_t(uint64_t v);

/*给定一个VirtAddr 转换为一个u64 */
uint64_t size_t_from_virt_addr(VirtAddr v);

/*给定一个VirtPageNum 转换为 u64*/
uint64_t size_t_from_virt_page_num(VirtPageNum v);

/* 物理地址向下取整 */
PhysPageNum floor_phys(PhysAddr phys_addr);

/* 物理地址向上取整 */
PhysPageNum ceil_phys(PhysAddr phys_addr);

/* 虚拟地址向下取整，得到虚拟页号 */
VirtPageNum floor_virts(VirtAddr virt_addr);

/* 把虚拟地址转换为虚拟页号 */
VirtPageNum virt_page_num_from_virt_addr(VirtAddr virt_addr);

/* 初始化分配器 */
void StackFrameAllocator_new(StackFrameAllocator *allocator);

/* 设置可分配物理页号范围 */
void StackFrameAllocator_init(
    StackFrameAllocator *allocator,
    PhysPageNum l,
    PhysPageNum r
);

/* 分配一个物理页 */
PhysPageNum StackFrameAllocator_alloc(
    StackFrameAllocator *allocator
);

/* 回收一个物理页 */
void StackFrameAllocator_dealloc(
    StackFrameAllocator *allocator,
    PhysPageNum ppn
);

void frame_allocator_test(void);

PageTableEntry PageTableEntry_new(PhysPageNum ppn, uint8_t PTEFlags);
PageTableEntry PageTableEntry_empty(void);
PhysPageNum PageTableEntry_ppn(PageTableEntry *entry);
uint8_t PageTableEntry_flags(PageTableEntry *entry);
bool PageTableEntry_is_valid(PageTableEntry *entry);

uint8_t* get_bytes_array(PhysPageNum ppn);
PageTableEntry* get_pte_array(PhysPageNum ppn);

void indexes(VirtPageNum vpn, size_t *result);

PageTableEntry *find_pte_create(PageTable *pt, VirtPageNum vpn);
PageTableEntry *find_pte(PageTable *pt, VirtPageNum vpn);

void PageTable_map(PageTable *pt, VirtAddr va,
                   PhysAddr pa, uint64_t size,
                   uint8_t pteflags);
void PageTable_unmap(PageTable *pt, VirtPageNum vpn);

void frame_allocator_init(void);

PageTable kvmmake(void);
void kvminit(void);
void kvminithart(void);

#endif
