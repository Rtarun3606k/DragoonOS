#include "elf.h"
#include "ff.h"
#include "heap.h"
#include "log.h"
#include "memory.h"
#include "paging.h"

#define PAGE_SIZE 4096U
#define USER_STACK_TOP 0xC0000000U
#define USER_STACK_PAGES 4U
#define MAX_ELF_FILE_SZ (4U * 1024U * 1024U)

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t read_cr3(void) {
  uint32_t value;

  __asm__ volatile("mov %%cr3, %0" : "=r"(value));

  return value;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int elf_validate_header(const elf32_ehdr_t *ehdr, unsigned int size) {
  if (size < sizeof(elf32_ehdr_t)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: image too small");
    return 0;
  }

  if (ehdr->e_ident_magic != ELF_MAGIC || ehdr->e_ident_class != 1 ||
      ehdr->e_type != ET_EXEC || ehdr->e_machine != EM_386) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "ELF: invalid 32-bit x86 executable");
    return 0;
  }

  if (ehdr->e_phoff + (uint32_t)ehdr->e_phnum * sizeof(elf32_phdr_t) > size) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "ELF: corrupt program header table");
    return 0;
  }

  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
int elf_load_image_into(unsigned int pd, const void *image, unsigned int size,
                        struct elf_load_info *info) {
  if (!pd || !image || !info) {
    return 0;
  }

  const elf32_ehdr_t *ehdr = (const elf32_ehdr_t *)image;

  if (!elf_validate_header(ehdr, size)) {
    return 0;
  }

  uint32_t old_cr3 = read_cr3();
  paging_switch_directory(pd);

  const uint8_t *raw = (const uint8_t *)image;
  const elf32_phdr_t *phdrs = (const elf32_phdr_t *)(raw + ehdr->e_phoff);
  uint32_t highest_vaddr = 0x08048000U;

  for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
    if (phdrs[i].p_type != PT_LOAD) {
      continue;
    }

    uint32_t vaddr = phdrs[i].p_vaddr;
    uint32_t filesz = phdrs[i].p_filesz;
    uint32_t memsz = phdrs[i].p_memsz;
    uint32_t offset = phdrs[i].p_offset;

    if (vaddr >= USER_STACK_TOP || memsz > USER_STACK_TOP - vaddr ||
        offset + filesz > size || filesz > memsz) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: invalid segment %u", i);
      goto fail;
    }

    if (vaddr + memsz > highest_vaddr) {
      highest_vaddr = vaddr + memsz;
    }

    uint32_t page_start = vaddr & ~(PAGE_SIZE - 1U);
    uint32_t page_end = (vaddr + memsz + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);

    for (uint32_t page = page_start; page < page_end; page += PAGE_SIZE) {
      if (!is_page_mapped(page)) {
        unsigned int frame = frame_alloc();

        if (!frame || map_page(page, frame, PAGE_WRITABLE | PAGE_USER) != 0) {
          log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: failed mapping 0x%x",
                      page);
          goto fail;
        }
      }
    }

    uint8_t *dst = (uint8_t *)vaddr;
    const uint8_t *src = raw + offset;

    for (uint32_t j = 0; j < filesz; j++) {
      dst[j] = src[j];
    }
    for (uint32_t j = filesz; j < memsz; j++) {
      dst[j] = 0;
    }
  }

  for (unsigned int i = 1; i <= USER_STACK_PAGES; i++) {
    uint32_t stack_page = USER_STACK_TOP - (i * PAGE_SIZE);
    unsigned int frame = frame_alloc();

    if (!frame || map_page(stack_page, frame, PAGE_WRITABLE | PAGE_USER) != 0) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                  "ELF: failed mapping user stack");
      goto fail;
    }
  }

  info->entry = ehdr->e_entry;
  info->stack_top = USER_STACK_TOP - 16U;
  info->brk_start = highest_vaddr;

  paging_switch_directory(old_cr3);
  return 1;

fail:
  paging_switch_directory(old_cr3);
  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
int elf_load_file_into(unsigned int pd, const char *path,
                       struct elf_load_info *info) {
  if (!pd || !path || !info) {
    return 0;
  }

  FIL fp;

  if (f_open(&fp, path, FA_READ) != FR_OK) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: cannot open %s", path);
    return 0;
  }

  FSIZE_t fsize = f_size(&fp);

  if (fsize < sizeof(elf32_ehdr_t) || fsize > MAX_ELF_FILE_SZ) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: invalid file size %u",
                (unsigned int)fsize);
    f_close(&fp);
    return 0;
  }

  uint8_t *buffer = (uint8_t *)kmalloc((unsigned int)fsize);

  if (!buffer) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: kmalloc failed for %s",
                path);
    f_close(&fp);
    return 0;
  }

  UINT bytes_read = 0;
  FRESULT result = f_read(&fp, buffer, (UINT)fsize, &bytes_read);
  f_close(&fp);

  if (result != FR_OK || bytes_read != (UINT)fsize) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: failed reading %s (fr=%d)",
                path, result);
    kfree(buffer);
    return 0;
  }

  if (!elf_validate_header((const elf32_ehdr_t *)buffer, (unsigned int)fsize)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ELF: %s is not a valid i386 ELF",
                path);
    kfree(buffer);
    return 0;
  }

  int loaded = elf_load_image_into(pd, buffer, (unsigned int)fsize, info);

  kfree(buffer);
  return loaded;
}
