#include "paging.h"
#include "memory.h"

#define PAGE_SIZE 4096U
#define PAGE_MASK 0xFFFFF000U
#define HIGHER_HALF 0xC0000000U
#define PAGE_ENTRIES 1024U
#define SCRATCH_PD_VIRT 0xDFFFF000U

extern unsigned char page_directory[];
static unsigned int kernel_pd_phys = 0;

/* With recursive paging, the active PD is always at 0xFFFFF000 */
static unsigned int *get_page_directory(void) {
  return (unsigned int *)0xFFFFF000U;
}

void paging_init(void) {
  kernel_pd_phys = (unsigned int)page_directory;
  unsigned int *pd =
      (unsigned int *)((unsigned int)page_directory + HIGHER_HALF);

  /* Map entry 1023 to the page directory itself for recursive access */
  pd[1023] = (unsigned int)page_directory | PAGE_PRESENT | PAGE_WRITABLE;

  /* Flush TLB */
  __asm__ volatile("mov %cr3, %eax; mov %eax, %cr3");
}

int map_page(unsigned int virtual_address, unsigned int physical_address,
             unsigned int flags) {
  if ((virtual_address & (PAGE_SIZE - 1U)) ||
      (physical_address & (PAGE_SIZE - 1U))) {
    return -1;
  }

  unsigned int directory_index = virtual_address >> 22;
  unsigned int table_index = (virtual_address >> 12) & 0x3FFU;

  unsigned int *directory = get_page_directory();
  unsigned int *table =
      (unsigned int *)(0xFFC00000U + (directory_index * PAGE_SIZE));

  if (!(directory[directory_index] & PAGE_PRESENT)) {
    unsigned int table_physical = frame_alloc();
    if (table_physical == 0) {
      return -1;
    }

    directory[directory_index] =
        table_physical | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

    __asm__ volatile("invlpg (%0)" : : "r"((unsigned int)table) : "memory");

    for (unsigned int i = 0; i < PAGE_ENTRIES; i++) {
      table[i] = 0;
    }
  } else if (flags & PAGE_USER) {
    directory[directory_index] |= PAGE_USER;
  }

  if (table[table_index] & PAGE_PRESENT) {
    return -1;
  }

  table[table_index] = physical_address | flags | PAGE_PRESENT;

  __asm__ volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");

  return 0;
}

unsigned int unmap_page(unsigned int virtual_address) {
  if (virtual_address & (PAGE_SIZE - 1U)) {
    return 0;
  }

  unsigned int directory_index = virtual_address >> 22;
  unsigned int table_index = (virtual_address >> 12) & 0x3FFU;

  unsigned int *directory = get_page_directory();

  if (!(directory[directory_index] & PAGE_PRESENT)) {
    return 0;
  }

  unsigned int *table =
      (unsigned int *)(0xFFC00000U + (directory_index * PAGE_SIZE));

  if (!table || !(table[table_index] & PAGE_PRESENT)) {
    return 0;
  }

  unsigned int physical_address = table[table_index] & PAGE_MASK;
  table[table_index] = 0;

  __asm__ volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");

  return physical_address;
}

int is_page_mapped(unsigned int virtual_address) {
  unsigned int directory_index = virtual_address >> 22;
  unsigned int table_index = (virtual_address >> 12) & 0x3FFU;
  unsigned int *directory = get_page_directory();

  if (!(directory[directory_index] & PAGE_PRESENT)) {
    return 0;
  }

  unsigned int *table =
      (unsigned int *)(0xFFC00000U + (directory_index * PAGE_SIZE));
  return (table[table_index] & PAGE_PRESENT) != 0;
}

unsigned int paging_create_user_directory(void) {
  unsigned int new_pd_phys = frame_alloc();
  if (new_pd_phys == 0) {
    return 0;
  }

  if (map_page(SCRATCH_PD_VIRT, new_pd_phys, PAGE_WRITABLE) != 0) {
    frame_free(new_pd_phys);
    return 0;
  }

  unsigned int *new_pd = (unsigned int *)SCRATCH_PD_VIRT;
  unsigned int *cur_pd = get_page_directory();

  /* Clear lower half (0x00000000 - 0xBFFFFFFF) for isolated user space */
  for (unsigned int i = 0; i < 768; i++) {
    new_pd[i] = 0;
  }

  /* Share higher-half kernel mappings (0xC0000000 - 0xFFBFFFFF) */
  for (unsigned int i = 768; i < 1023; i++) {
    new_pd[i] = cur_pd[i];
  }

  /* Point entry 1023 to the new PD itself so recursive paging works in user CR3
   */
  new_pd[1023] = new_pd_phys | PAGE_PRESENT | PAGE_WRITABLE;

  unmap_page(SCRATCH_PD_VIRT);
  return new_pd_phys;
}

void paging_switch_directory(unsigned int pd_physical) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(pd_physical) : "memory");
}

void paging_switch_to_kernel_directory(void) {
  if (kernel_pd_phys != 0) {
    paging_switch_directory(kernel_pd_phys);
  }
}
