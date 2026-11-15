#ifndef PAGING_H
#define PAGING_H

#define PAGE_PRESENT  0x001U
#define PAGE_WRITABLE 0x002U
#define PAGE_USER     0x004U

void paging_init(void);
int map_page(unsigned int virtual_address, unsigned int physical_address, unsigned int flags);
unsigned int unmap_page(unsigned int virtual_address);
int is_page_mapped(unsigned int virtual_address);

unsigned int paging_create_user_directory(void);
void paging_switch_directory(unsigned int pd_physical);
void paging_switch_to_kernel_directory(void);

#endif
