#ifndef MULTIBOOT_H
#define MULTIBOOT_H

typedef struct {
  unsigned int mod_start;
  unsigned int mod_end;
  unsigned int string;
  unsigned int reserved;
} multiboot_module_t;

typedef struct {
  unsigned int flags;
  unsigned int mem_lower;
  unsigned int mem_upper;
  unsigned int boot_device;
  unsigned int cmdline;
  unsigned int mods_count;
  unsigned int mods_addr;

  /* Four words reserved for Multiboot symbol information. */
  unsigned int syms[4];

  unsigned int mmap_length;
  unsigned int mmap_addr;
} multiboot_info_t;

typedef struct __attribute__((packed)) {
  unsigned int size;
  unsigned int addr_low;
  unsigned int addr_high;
  unsigned int len_low;
  unsigned int len_high;
  unsigned int type;
} multiboot_mmap_entry_t;

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MODULES (1 << 3)
#define MULTIBOOT_INFO_MEMORY (1 << 0)
#define MULTIBOOT_INFO_MMAP (1 << 6)

#define MULTIBOOT_MEMORY_AVAILABLE 1

#endif
