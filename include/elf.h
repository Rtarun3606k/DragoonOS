#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC 0x464C457FU /* "\x7FELF" in little-endian */
#define ET_EXEC 2U
#define EM_386 3U
#define PT_LOAD 1U

/* User-space layout shared by the ELF loader and the argv builder. */
#define USER_STACK_TOP 0xC0000000U
#define USER_STACK_SIZE (4U * 4096U)

typedef struct {
  uint32_t e_ident_magic;
  uint8_t e_ident_class;
  uint8_t e_ident_data;
  uint8_t e_ident_version;
  uint8_t e_ident_osabi;
  uint8_t e_ident_pad[8];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

/* Address-space layout produced by a successful load. */
struct elf_load_info {
  uint32_t entry;
  uint32_t stack_top;
  uint32_t brk_start;
};

/*
 * Loads an ELF image into an existing user page directory.  The directory is
 * activated temporarily (the kernel half is shared) and restored afterwards.
 * The image buffer must live in kernel-shared memory.
 */
int elf_load_image_into(unsigned int pd, const void *image, unsigned int size,
                        struct elf_load_info *info);

/* Reads path from FAT32 and loads it into pd. */
int elf_load_file_into(unsigned int pd, const char *path,
                       struct elf_load_info *info);

#endif
