#ifndef SYSCALL_H
#define SYSCALL_H

#include "interrupts.h"

#define SYS_EXIT 1U
#define SYS_READ 3U
#define SYS_WRITE 4U
#define SYS_CLEAR 5U
#define SYS_MEMINFO 6U

struct user_meminfo {
  unsigned int free_frames;
  unsigned int frame_size;
};

void syscall_handler(struct interrupt_frame *frame);

#endif
