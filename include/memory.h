#ifndef MEMORY_H
#define MEMORY_H

#define PAGE_SIZE 4096U

void memory_init(unsigned int multiboot_info_address);

unsigned int frame_alloc(void);
void frame_free(unsigned int physical_address);

unsigned int memory_get_free_frames(void);
#endif
