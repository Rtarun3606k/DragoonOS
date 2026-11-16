#ifndef SYSCALL_H
#define SYSCALL_H

#include "interrupts.h"
#include <stdint.h>

#define SYS_EXIT 1U
#define SYS_READ 3U
#define SYS_WRITE 4U
#define SYS_CLEAR 5U
#define SYS_MEMINFO 6U
#define SYS_OPEN 7U
#define SYS_CLOSE 8U
#define SYS_READDIR 9U
#define SYS_BRK 10U
#define SYS_EXEC 11U
#define SYS_SPAWN 12U
#define SYS_WAIT 13U

#define O_RDONLY 0x01U
#define O_WRONLY 0x02U
#define O_RDWR 0x03U
#define O_CREAT 0x04U
#define O_TRUNC 0x08U
#define O_APPEND 0x10U

struct user_meminfo {
  unsigned int free_frames;
  unsigned int frame_size;
};

struct user_dirent {
  char name[64];
  uint32_t size;
  uint8_t is_dir;
};

void syscall_handler(struct interrupt_frame *frame);

/* Terminates the current user process after an unrecoverable fault. */
void syscall_terminate_user(int status);

#endif
