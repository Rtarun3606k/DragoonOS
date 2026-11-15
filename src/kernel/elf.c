#include "elf.h"
#include "gdt.h"
#include "heap.h"
#include "log.h"
#include "memory.h"
#include "paging.h"

#define PAGE_SIZE        4096U
#define USER_STACK_TOP   0xC0000000U
#define USER_STACK_PAGES 4U

int elf_load_and_exec(const void *image, unsigned int size) {
  if (!image || size < sizeof(elf32_ehdr_t)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: image too small");
    return 0;
  }

  const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)image;

  if (ehdr->e_ident_magic != ELF_MAGIC || ehdr->e_ident_class != 1 ||
      ehdr->e_type != ET_EXEC || ehdr->e_machine != EM_386) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: invalid 32-bit x86 executable");
    return 0;
  }

  if (ehdr->e_phoff + (uint32_t)ehdr->e_phnum * sizeof(elf32_phdr_t) > size) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: corrupt program header table");
    return 0;
  }

  unsigned int user_pd = paging_create_user_directory();
  if (!user_pd) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: failed to create user page directory");
    return 0;
  }

  /* Switch into the isolated user address space while keeping higher-half kernel mapped */
  paging_switch_directory(user_pd);

  const uint8_t *raw = (const uint8_t *)image;
  const elf32_phdr_t *phdrs = (const elf32_phdr_t *)(raw + ehdr->e_phoff);

  for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
    if (phdrs[i].p_type != PT_LOAD) {
      continue;
    }

    uint32_t vaddr  = phdrs[i].p_vaddr;
    uint32_t filesz = phdrs[i].p_filesz;
    uint32_t memsz  = phdrs[i].p_memsz;
    uint32_t offset = phdrs[i].p_offset;

    if (vaddr >= USER_STACK_TOP || memsz > USER_STACK_TOP - vaddr ||
        offset + filesz > size || filesz > memsz) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: invalid segment %u", i);
      paging_switch_to_kernel_directory();
      return 0;
    }

    uint32_t page_start = vaddr & ~(PAGE_SIZE - 1U);
    uint32_t page_end   = (vaddr + memsz + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);

    for (uint32_t page = page_start; page < page_end; page += PAGE_SIZE) {
      if (!is_page_mapped(page)) {
        unsigned int frame = frame_alloc();
        if (!frame || map_page(page, frame, PAGE_WRITABLE | PAGE_USER) != 0) {
          log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: failed mapping 0x%x", page);
          paging_switch_to_kernel_directory();
          return 0;
        }
      }
    }

    /* Copy initialized segment bytes (.text / .rodata / .data) */
    uint8_t *dst = (uint8_t *)vaddr;
    const uint8_t *src = raw + offset;
    for (uint32_t j = 0; j < filesz; j++) {
      dst[j] = src[j];
    }

    /* Zero out .bss tail */
    for (uint32_t j = filesz; j < memsz; j++) {
      dst[j] = 0;
    }
  }

  /* Map 16 KiB user stack right below 0xC0000000 */
  for (unsigned int i = 1; i <= USER_STACK_PAGES; i++) {
    uint32_t stack_page = USER_STACK_TOP - (i * PAGE_SIZE);
    unsigned int frame = frame_alloc();
    if (!frame || map_page(stack_page, frame, PAGE_WRITABLE | PAGE_USER) != 0) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: failed mapping user stack");
      paging_switch_to_kernel_directory();
      return 0;
    }
  }

  void *kstack = kmalloc(4096);
  if (!kstack) {
    paging_switch_to_kernel_directory();
    return 0;
  }
  tss_set_stack((unsigned int)kstack + 4096U);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "ELF: launching Ring 3 entry 0x%x", ehdr->e_entry);
  enter_usermode(ehdr->e_entry, USER_STACK_TOP - 16U);

  return 1;
}
