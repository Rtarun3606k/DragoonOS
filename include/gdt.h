// global description table
#ifndef GDT_H
#define GDT_H

void gdt_initialize(void);
void tss_set_stack(unsigned int kernel_esp);
void enter_usermode(unsigned int entry_eip, unsigned int user_esp);

#endif
