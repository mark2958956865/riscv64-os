#ifndef HW_RISCV_QUARD_STAR_H
#define HW_RISCV_QUARD_STAR_H

#include "hw/core/boards.h"
#include "hw/core/qdev.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/block/flash.h"
#include "qom/object.h"

#define QUARD_STAR_CPUS_MAX_BITS        3
#define QUARD_STAR_CPUS_MAX             (1 << QUARD_STAR_CPUS_MAX_BITS)
#define QUARD_STAR_SOCKETS_MAX_BITS     3
#define QUARD_STAR_SOCKETS_MAX          (1 << QUARD_STAR_SOCKETS_MAX_BITS)

#define TYPE_RISCV_QUARD_STAR_MACHINE MACHINE_TYPE_NAME("quard-star")
typedef struct QuardStarState QuardStarState;
DECLARE_INSTANCE_CHECKER(QuardStarState,QUARD_STAR_MACHINE,TYPE_RISCV_QUARD_STAR_MACHINE)

struct QuardStarState{

    MachineState parent;

    RISCVHartArrayState soc[QUARD_STAR_SOCKETS_MAX];
    PFlashCFI01 *flash;
    DeviceState *plic[QUARD_STAR_SOCKETS_MAX];

};

enum {
    QUARD_STAR_MROM,
    QUARD_STAR_SRAM,
    QUARD_STAR_CLINT,
    QUARD_STAR_PLIC,
    QUARD_STAR_UART0,
    QUARD_STAR_UART1,
    QUARD_STAR_UART2,
    QUARD_STAR_RTC,
    QUARD_STAR_FLASH,
    QUARD_STAR_DRAM,
};

enum{
    QUARD_STAR_UART0_IRQ = 10,
    QUARD_STAR_UART1_IRQ = 11,
    QUARD_STAR_UART2_IRQ = 12,
    QUARD_STAR_RTC_IRQ   = 13,

};

#define QUARD_STAR_PLIC_NUM_SOURCES    127      //PLIC 支持的中断源的最大数量
#define QUARD_STAR_PLIC_NUM_PRIORITIES 7        //PLIC 支持的中断优先级的数量
#define QUARD_STAR_PLIC_PRIORITY_BASE  0x04     //PLIC 中断优先级寄存器的基址偏移值，用于访问中断优先级信息
#define QUARD_STAR_PLIC_PENDING_BASE   0x1000   //PLIC 中断挂起寄存器的基址偏移值，用于访问中断挂起状态
#define QUARD_STAR_PLIC_ENABLE_BASE    0x2000   //PLIC 中断使能寄存器的基址偏移值，用于控制中断使能状态
#define QUARD_STAR_PLIC_ENABLE_STRIDE  0x80     //PLIC 中断使能寄存器之间的地址间隔
#define QUARD_STAR_PLIC_CONTEXT_BASE   0x200000 //PLIC 上下文保存寄存器的基址偏移值，用于保存中断处理程序的上下文信息
#define QUARD_STAR_PLIC_CONTEXT_STRIDE 0x1000   //PLIC 上下文保存寄存器之间的地址间隔
#define QUARD_STAR_PLIC_SIZE(__num_context) \
    (QUARD_STAR_PLIC_CONTEXT_BASE + (__num_context) * QUARD_STAR_PLIC_CONTEXT_STRIDE)


#endif







