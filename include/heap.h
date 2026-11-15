#ifndef HEAP_H
#define HEAP_H

void *kmalloc(unsigned int size);
void kfree(void *ptr);
void *krealloc(void *ptr, unsigned int size);
void heap_test(void);

#endif
