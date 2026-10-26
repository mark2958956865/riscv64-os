#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qapi/error.h"

#include "exec/hwaddr.h"

#include "hw/core/sysbus.h"
#include "hw/core/qdev-properties.h"

#include "hw/char/serial-mm.h"
#include "chardev/char.h"
#include "hw/rtc/goldfish_rtc.h"

#include "hw/intc/sifive_plic.h"
#include "hw/intc/riscv_aclint.h"

#include "hw/riscv/quard_star.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/numa.h"
#include "hw/riscv/machines-qom.h"

#include "target/riscv/cpu.h"

#include "system/memory.h"
#include "system/address-spaces.h"
#include "system/blockdev.h"
#include "system/system.h"

static const MemMapEntry quard_star_memmap[] = {
    [QUARD_STAR_MROM]  =        {        0x0,        0x8000 },
    [QUARD_STAR_SRAM]  =        {     0x8000,        0x8000 },
    [QUARD_STAR_CLINT] =        { 0x02000000,       0x10000 },
    [QUARD_STAR_PLIC]  =        { 0x0c000000,     0x4000000 },
    [QUARD_STAR_UART0] =        { 0x10000000,         0x100 },
    [QUARD_STAR_UART1] =        { 0x10001000,         0x100 },
    [QUARD_STAR_UART2] =        { 0x10002000,         0x100 },
    [QUARD_STAR_RTC]   =        { 0x10003000,        0x1000 },
    [QUARD_STAR_FLASH] =        { 0x20000000,     0x2000000 },
    [QUARD_STAR_DRAM]  =        { 0x80000000,           0x0 },
};

static void quard_star_cpu_create(MachineState *machine)
{
    QuardStarState *s = QUARD_STAR_MACHINE(machine);
    int i, base_hartid, hart_count;
    int socket_count = riscv_socket_count(machine);

    if (QUARD_STAR_SOCKETS_MAX < socket_count) {
        error_report("number of sockets/nodes should be less than %d", 
            QUARD_STAR_SOCKETS_MAX);
        exit(1);
    }

    for (i = 0; i < socket_count; i++){
        g_autofree char *soc_name = g_strdup_printf("soc%d", i);

        if (!riscv_socket_check_hartids(machine, i)) {
            error_report("discontinuous hartids in socket%d", i);
            exit(1);
        }

        base_hartid = riscv_socket_first_hartid(machine, i);
        if (base_hartid < 0) {
            error_report("can't find hartid base for socket%d", i);
            exit(1);
        }

        hart_count = riscv_socket_hart_count(machine, i);
        if (hart_count < 0) {
            error_report("can't find hart count for socket%d", i);
            exit(1);
        }

        object_initialize_child(OBJECT(machine), soc_name, &s->soc[i], 
                                TYPE_RISCV_HART_ARRAY);
        object_property_set_str(OBJECT(&s->soc[i]), "cpu-type", 
                                machine->cpu_type, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "hartid-base",
                                base_hartid, &error_abort);
        object_property_set_int(OBJECT(&s->soc[i]), "num-harts",
                                hart_count, &error_abort);
        qdev_prop_set_uint64(DEVICE(&s->soc[i]), "resetvec", quard_star_memmap[QUARD_STAR_MROM].base);
        sysbus_realize(SYS_BUS_DEVICE(&s->soc[i]), &error_fatal);
    }
}

#define QUARD_STAR_FLASH_SECTOR_SIZE (256 * KiB)

static void quard_star_flash_create(QuardStarState *s)
{
    DeviceState *dev = qdev_new(TYPE_PFLASH_CFI01);

    qdev_prop_set_uint64(dev, "sector-length", QUARD_STAR_FLASH_SECTOR_SIZE);
    qdev_prop_set_uint8(dev, "width", 4);
    qdev_prop_set_uint8(dev, "device-width", 2);
    qdev_prop_set_bit(dev, "big-endian", false);
    qdev_prop_set_uint16(dev, "id0", 0x89);
    qdev_prop_set_uint16(dev, "id1", 0x18);
    qdev_prop_set_uint16(dev, "id2", 0x00);
    qdev_prop_set_uint16(dev, "id3", 0x00);
    qdev_prop_set_string(dev, "name", "quard-star.flash0");

    object_property_add_child(OBJECT(s), "quard-star.flash0", OBJECT(dev));
    object_property_add_alias(OBJECT(s), "flash0", OBJECT(dev),"drive");

    s->flash = PFLASH_CFI01(dev);
}

static void quard_star_flash_map(MachineState *machine)
{
    QuardStarState *s = QUARD_STAR_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    DeviceState *dev = DEVICE(s->flash);

    hwaddr flashsize = quard_star_memmap[QUARD_STAR_FLASH].size;
    hwaddr flashbase = quard_star_memmap[QUARD_STAR_FLASH].base;

    pflash_cfi01_legacy_drive(s->flash,drive_get(IF_PFLASH, 0, 0));

    assert(QEMU_IS_ALIGNED(flashsize, QUARD_STAR_FLASH_SECTOR_SIZE));
    assert(flashsize / QUARD_STAR_FLASH_SECTOR_SIZE <= UINT32_MAX);
    qdev_prop_set_uint32(dev, "num-blocks", flashsize / QUARD_STAR_FLASH_SECTOR_SIZE);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    memory_region_add_subregion(system_memory, flashbase,
                                sysbus_mmio_get_region(SYS_BUS_DEVICE(dev),
                                                       0));
}

static void quard_star_memory_create(MachineState *machine)
{
    QuardStarState *s = QUARD_STAR_MACHINE(machine);
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram_mem = g_new(MemoryRegion, 1);
    MemoryRegion *mask_rom = g_new(MemoryRegion, 1);
    memory_region_add_subregion(system_memory, quard_star_memmap[QUARD_STAR_DRAM].base,
                                machine->ram);
    memory_region_init_ram(sram_mem, NULL, "riscv_quard_star_board.sram",
                           quard_star_memmap[QUARD_STAR_SRAM].size, &error_fatal);
    memory_region_add_subregion(system_memory, quard_star_memmap[QUARD_STAR_SRAM].base,
                                sram_mem);                           
    memory_region_init_rom(mask_rom, NULL, "riscv_quard_star_board.mrom",
                           quard_star_memmap[QUARD_STAR_MROM].size, &error_fatal);
    memory_region_add_subregion(system_memory, quard_star_memmap[QUARD_STAR_MROM].base,
                                mask_rom);

    riscv_setup_rom_reset_vec(machine, &s->soc[0],
                              quard_star_memmap[QUARD_STAR_FLASH].base,
                              quard_star_memmap[QUARD_STAR_MROM].base,
                              quard_star_memmap[QUARD_STAR_MROM].size,
                              0x0, 0x0);
}

static void quard_star_plic_create(MachineState *machine)
{
    int i, hart_count, base_hartid;
    int socket_count = riscv_socket_count(machine);
    QuardStarState *s = QUARD_STAR_MACHINE(machine);
    for ( i = 0; i < socket_count; i++ ){
        hart_count = riscv_socket_hart_count(machine, i);
        base_hartid = riscv_socket_first_hartid(machine,i);
        g_autofree char *plic_hart_config = riscv_plic_hart_config_string(hart_count);

        s->plic[i] = sifive_plic_create(
            quard_star_memmap[QUARD_STAR_PLIC].base + i *quard_star_memmap[QUARD_STAR_PLIC].size ,
            plic_hart_config, hart_count , base_hartid,
            QUARD_STAR_PLIC_NUM_SOURCES,
            QUARD_STAR_PLIC_NUM_PRIORITIES,
            QUARD_STAR_PLIC_PRIORITY_BASE,
            QUARD_STAR_PLIC_PENDING_BASE,
            QUARD_STAR_PLIC_ENABLE_BASE,
            QUARD_STAR_PLIC_ENABLE_STRIDE,
            QUARD_STAR_PLIC_CONTEXT_BASE,
            QUARD_STAR_PLIC_CONTEXT_STRIDE,
            quard_star_memmap[QUARD_STAR_PLIC].size);
    }
}

static void quard_star_aclint_create(MachineState *machine)
{
    int i, hart_count, base_hartid;
    int socket_count = riscv_socket_count(machine);

    for ( i = 0; i < socket_count; i++ ){
        hart_count = riscv_socket_hart_count(machine, i);
        base_hartid = riscv_socket_first_hartid(machine,i);

        riscv_aclint_swi_create(quard_star_memmap[QUARD_STAR_CLINT].base +
                        i * quard_star_memmap[QUARD_STAR_CLINT].size,
                    base_hartid, hart_count, false);
        riscv_aclint_mtimer_create(quard_star_memmap[QUARD_STAR_CLINT].base +
                    i * quard_star_memmap[QUARD_STAR_CLINT].size +
                    RISCV_ACLINT_SWI_SIZE,
                RISCV_ACLINT_DEFAULT_MTIMER_SIZE,
                base_hartid, hart_count,
                RISCV_ACLINT_DEFAULT_MTIMECMP,
                RISCV_ACLINT_DEFAULT_MTIME,
                RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ, true);
    }
}

static void quard_star_serial_create(MachineState *machine)
{
    MemoryRegion *system_memory = get_system_memory();
    QuardStarState *s = QUARD_STAR_MACHINE(machine);

    serial_mm_init(system_memory,quard_star_memmap[QUARD_STAR_UART0].base,
        0, qdev_get_gpio_in(DEVICE(s->plic[0]), QUARD_STAR_UART0_IRQ), 399193,
        serial_hd(0), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory,quard_star_memmap[QUARD_STAR_UART1].base,
        0, qdev_get_gpio_in(DEVICE(s->plic[0]), QUARD_STAR_UART1_IRQ), 399193,
        serial_hd(1), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory,quard_star_memmap[QUARD_STAR_UART2].base,
        0, qdev_get_gpio_in(DEVICE(s->plic[0]), QUARD_STAR_UART2_IRQ), 399193,
        serial_hd(2), DEVICE_LITTLE_ENDIAN);
}

static void quard_star_rtc_create(MachineState *machine)
{
    QuardStarState *s = QUARD_STAR_MACHINE(machine);
    
    sysbus_create_simple("goldfish_rtc", quard_star_memmap[QUARD_STAR_RTC].base,
        qdev_get_gpio_in(DEVICE(s->plic[0]), QUARD_STAR_RTC_IRQ));
}

static void quard_star_machine_init(MachineState *machine)
{
    quard_star_cpu_create(machine);
    quard_star_memory_create(machine);
    quard_star_flash_map(machine);
    quard_star_plic_create(machine);
    quard_star_aclint_create(machine);
    quard_star_serial_create(machine);
    quard_star_rtc_create(machine);
}

static void quard_star_machine_instance_init(Object *obj)
{
    QuardStarState *s = QUARD_STAR_MACHINE(obj);
    quard_star_flash_create(s);

}

static void quard_star_machine_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "RISC-V Quard Star board";
    mc->init = quard_star_machine_init;
    mc->max_cpus = QUARD_STAR_CPUS_MAX;
    mc->default_cpu_type = TYPE_RISCV_CPU_BASE;
    mc->pci_allow_0_address = true;
    mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;
    mc->cpu_index_to_instance_props = riscv_numa_cpu_index_to_props;
    mc->get_default_cpu_node_id = riscv_numa_get_default_cpu_node_id;
    mc->numa_mem_supported = true;
    mc->default_ram_id = "riscv_quard_star_board.ram";
}

static const TypeInfo quard_star_machine_typeinfo = {
    .name       = MACHINE_TYPE_NAME("quard-star"),
    .parent     = TYPE_MACHINE,
    .class_init = quard_star_machine_class_init,
    .instance_init = quard_star_machine_instance_init,
    .instance_size = sizeof(QuardStarState),
    .interfaces = (const InterfaceInfo[]) {
         { TYPE_HOTPLUG_HANDLER },
         { TYPE_TARGET_RISCV64_MACHINE },
         { }
    },
};

static void quard_star_machine_init_register_types(void)
{
    type_register_static(&quard_star_machine_typeinfo);
}
type_init(quard_star_machine_init_register_types);

