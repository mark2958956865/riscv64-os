#ifndef TOS_LOADER_H
#define TOS_LOADER_H

#include <timeros/types.h>

//前16个字节
#define EI_NIDENT 16
//所有的ASCII码都可以用"\加数字（一般是8进制数字）来表示。
#define ELFMAG 0x464C457FU  // 0x464C457FU "\177ELF"

#define EM_RISCV 0xF3 //risc-v 对应的 e_machine 的value值
#define EI_CLASS 4 //EI_NIDENT 的 第四位
#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2
#define ELFCLASSNUM 3

#define PT_LOAD 1

//ELF文件中段属性定义
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

//elf header 结构体定义
typedef struct {
    uint8_t e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

//program header 结构体定义
typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;

// 保存应用程序的元数据
typedef struct {
    uint64_t start;  // ELF 文件在内核中的起始地址
    uint64_t size;   // ELF 文件数据的长度
} AppMetadata;

size_t get_num_app(void);
AppMetadata get_app_data(size_t app_id);
void load_app(size_t app_id);
void get_app_names(void);
AppMetadata get_app_data_by_name(const char *name);
int exec(const char *name);

#endif