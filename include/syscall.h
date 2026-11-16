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
#define SYS_LSEEK 14U
#define SYS_STAT 15U
#define SYS_FSTAT 16U
#define SYS_MKDIR 17U
#define SYS_UNLINK 18U

/*
 * SYS_SPAWN (12): EBX = path, ECX = argv (NULL-terminated array of user
 * strings, may be NULL).  Returns the child pid, or -1 on failure.
 * SYS_WAIT (13):  EBX = pid, ECX = optional status pointer.
 * SYS_LSEEK (14): EBX = fd, ECX = signed offset, EDX = whence
 *                 (0 = SET, 1 = CUR, 2 = END).
 * SYS_STAT (15):  EBX = path, ECX = struct user_stat pointer.
 * SYS_FSTAT (16): EBX = fd, ECX = struct user_stat pointer.
 * SYS_MKDIR (17): EBX = path.
 * SYS_UNLINK (18): EBX = path (file or empty directory).
 */

#define O_RDONLY 0x01U
#define O_WRONLY 0x02U
#define O_RDWR 0x03U
#define O_CREAT 0x04U
#define O_TRUNC 0x08U
#define O_APPEND 0x10U

#define LSEEK_SET 0U
#define LSEEK_CUR 1U
#define LSEEK_END 2U

struct user_meminfo {
  unsigned int free_frames;
  unsigned int frame_size;
};

struct user_dirent {
  char name[64];
  uint32_t size;
  uint8_t is_dir;
};

/*
 * Compact FAT32 status record shared with userland.  Field offsets must stay
 * in sync with struct dragoon_stat in the TinyGo os layer.
 */
struct user_stat {
  uint32_t size;
  uint16_t date; /* FAT date */
  uint16_t time; /* FAT time */
  uint8_t is_dir;
  uint8_t is_readonly;
  uint16_t reserved;
};

void syscall_handler(struct interrupt_frame *frame);

/* Terminates the current user process after an unrecoverable fault. */
void syscall_terminate_user(int status);

#endif
