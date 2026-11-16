#include "syscall.h"
#include "elf.h"
#include "framebuffer.h"
#include "heap.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "paging.h"
#include "serial.h"
#include "syscallFS.h"
#include "task.h"

#define USER_SPACE_LIMIT 0xC0000000U
#define USER_BRK_MAX 0xB0000000U
#define USER_PATH_MAX 256U

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int is_valid_user_buffer(uint32_t ptr, uint32_t len) {
  if (ptr == 0 || ptr >= USER_SPACE_LIMIT) {
    return 0;
  }
  if (len > USER_SPACE_LIMIT - ptr) {
    return 0;
  }
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int user_range_mapped(uint32_t ptr, uint32_t len) {
  if (len == 0) {
    return 1;
  }
  if (!is_valid_user_buffer(ptr, len)) {
    return 0;
  }

  uint32_t last = ptr + len - 1U;

  for (uint32_t page = ptr & ~(PAGE_SIZE - 1U); page <= last;
       page += PAGE_SIZE) {
    if (!is_page_mapped(page)) {
      return 0;
    }
  }

  return 1;
}

//-------------------------------------------------------------------------------------
// Copies a NUL-terminated string from user space, validating every page it
// touches so a bad pointer cannot fault in kernel mode.
//-------------------------------------------------------------------------------------
static int copy_user_string(uint32_t addr, char *dst, uint32_t capacity) {
  if (addr < 0x1000U || addr >= USER_SPACE_LIMIT || capacity == 0) {
    return 0;
  }

  for (uint32_t i = 0; i + 1U < capacity; i++) {
    uint32_t cursor = addr + i;

    if (cursor >= USER_SPACE_LIMIT) {
      return 0;
    }

    if (i == 0 || (cursor & (PAGE_SIZE - 1U)) == 0) {
      if (!is_page_mapped(cursor & ~(PAGE_SIZE - 1U))) {
        return 0;
      }
    }

    char c = ((const char *)cursor)[0];
    dst[i] = c;

    if (c == '\0') {
      return 1;
    }
  }

  dst[capacity - 1U] = '\0';
  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t sys_read(uint32_t fd, uint32_t buf_addr, uint32_t len) {
  if (fd >= SYSCALL_FS_FD_OFFSET) {
    return syscall_fs_read(fd, buf_addr, len);
  }

  if (fd != 0) {
    return (uint32_t)-1;
  }

  if (len == 0) {
    return 0;
  }

  if (!user_range_mapped(buf_addr, len)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_read: invalid user buffer 0x%x (len=%u)", buf_addr, len);
    return (uint32_t)-1;
  }

  char *user_buf = (char *)buf_addr;

  /*
   * Canonical stdin: return whatever has been typed so far (at least one
   * line), or park the task until IRQ1 pushes data / an EOF marker.  The
   * keyboard handler wakes us via task_wake_stdin().
   */
  for (;;) {
    int count = keyboard_stdin_read(user_buf, len);

    if (count > 0) {
      return (uint32_t)count;
    }

    if (keyboard_stdin_eof()) {
      keyboard_stdin_clear_eof();
      return 0;
    }

    struct task *task = task_current();

    if (!task) {
      return (uint32_t)-1;
    }

    task->wait_stdin = 1;
    task->state = TASK_WAITING;
    task_schedule();
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t len) {
  if (fd >= SYSCALL_FS_FD_OFFSET) {
    return syscall_fs_write(fd, buf_addr, len);
  }

  if (len == 0) {
    return 0;
  }

  if (!is_valid_user_buffer(buf_addr, len)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_write: invalid user buffer 0x%x (len=%u)", buf_addr, len);
    return (uint32_t)-1;
  }

  const char *buf = (const char *)buf_addr;

  if (fd == 1 || fd == 2) {
    fb_write(buf, len);
    serial_write(buf, len);
    return len;
  }

  return (uint32_t)-1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t sys_meminfo(uint32_t info_addr) {
  if (!is_valid_user_buffer(info_addr, sizeof(struct user_meminfo))) {
    return (uint32_t)-1;
  }

  struct user_meminfo *info = (struct user_meminfo *)info_addr;
  info->free_frames = memory_get_free_frames();
  info->frame_size = 4096U;
  return 0;
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
static uint32_t sys_brk(uint32_t new_brk) {
  struct task *task = task_current();

  if (!task) {
    return 0;
  }

  if (new_brk == 0) {
    return task->brk_cur;
  }

  if (new_brk < task->brk_min || new_brk >= USER_BRK_MAX) {
    return task->brk_cur;
  }

  if (new_brk > task->brk_cur) {
    uint32_t old_page = (task->brk_cur + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
    uint32_t new_page = (new_brk + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);

    for (uint32_t vaddr = old_page; vaddr < new_page; vaddr += PAGE_SIZE) {
      if (!is_page_mapped(vaddr)) {
        unsigned int phys = frame_alloc();
        if (phys == 0) {
          log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                      "sys_brk: out of physical frames at 0x%x", vaddr);
          return task->brk_cur;
        }
        if (map_page(vaddr, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER) !=
            0) {
          frame_free(phys);
          return task->brk_cur;
        }
        uint8_t *p = (uint8_t *)vaddr;
        for (uint32_t i = 0; i < PAGE_SIZE; i++) {
          p[i] = 0;
        }
      }
    }
  }

  task->brk_cur = new_brk;
  return task->brk_cur;
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
static uint32_t sys_spawn(uint32_t path_addr, uint32_t argv_addr) {
  char path[USER_PATH_MAX];
  const char *kernel_argv[MAX_ARGS + 1];
  char *arg_store = 0;
  int argc = 0;

  if (!copy_user_string(path_addr, path, sizeof(path))) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_spawn: invalid path pointer 0x%x", path_addr);
    return (uint32_t)-1;
  }

  kernel_argv[0] = 0;

  if (argv_addr != 0) {
    for (int i = 0; i <= MAX_ARGS; i++) {
      uint32_t slot = argv_addr + (uint32_t)i * sizeof(uint32_t);

      if (!user_range_mapped(slot, sizeof(uint32_t))) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "sys_spawn: invalid argv pointer 0x%x", slot);
        goto fail;
      }

      uint32_t user_arg = *(const uint32_t *)slot;

      if (user_arg == 0) {
        kernel_argv[i] = 0;
        break;
      }

      if (i == MAX_ARGS) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "sys_spawn: too many arguments (max %d)", MAX_ARGS);
        goto fail;
      }

      if (!arg_store) {
        arg_store = (char *)kmalloc((unsigned int)MAX_ARGS * MAX_ARG_LEN);
        if (!arg_store) {
          log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                      "sys_spawn: out of memory copying argv");
          goto fail;
        }
      }

      char *dst = arg_store + (uint32_t)i * MAX_ARG_LEN;

      if (!copy_user_string(user_arg, dst, MAX_ARG_LEN)) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "sys_spawn: invalid argument pointer 0x%x", user_arg);
        goto fail;
      }

      kernel_argv[i] = dst;
      argc++;
    }

    kernel_argv[argc] = 0;
  }

  struct task *child;

  if (!task_spawn(path, argv_addr != 0 ? kernel_argv : 0, &child)) {
    goto fail;
  }

  if (arg_store) {
    kfree(arg_store);
  }

  return (uint32_t)child->pid;

fail:
  if (arg_store) {
    kfree(arg_store);
  }

  return (uint32_t)-1;
}

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
static uint32_t sys_wait(uint32_t pid, uint32_t status_addr) {
  int has_status = status_addr != 0;

  if (has_status && !user_range_mapped(status_addr, sizeof(int))) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_wait: invalid status pointer 0x%x", status_addr);
    return (uint32_t)-1;
  }

  int exit_code = 0;
  int child = task_wait_pid((int)pid, &exit_code);

  if (child < 0) {
    return (uint32_t)-1;
  }

  if (has_status) {
    *(int *)status_addr = exit_code;
  }

  return (uint32_t)child;
}

//---------------------------------------------------------------------------------------------------
// Legacy SYS_EXEC: spawn the program as a child and wait for it, leaving the
// calling process and its address space intact.
//---------------------------------------------------------------------------------------------------
static uint32_t sys_exec(uint32_t path_addr) {
  char path[USER_PATH_MAX];

  if (!copy_user_string(path_addr, path, sizeof(path))) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_exec: invalid path pointer 0x%x", path_addr);
    return (uint32_t)-1;
  }

  struct task *child;

  if (!task_spawn(path, 0, &child)) {
    return (uint32_t)-1;
  }

  int exit_code = 0;

  if (task_wait_pid(child->pid, &exit_code) < 0) {
    return (uint32_t)-1;
  }

  return (uint32_t)exit_code;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void sys_exit(int status) {
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "User process %d exited with status: %d",
              task_current_pid(), status);

  task_exit_current(status);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void syscall_terminate_user(int status) {
  log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
              "Terminating user process %d with status: %d",
              task_current_pid(), status);

  task_exit_current(status);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void syscall_handler(struct interrupt_frame *frame) {
  uint32_t num = frame->eax;

  switch (num) {
  case SYS_EXIT:
    sys_exit((int)frame->ebx);
    break;

  case SYS_READ:
    frame->eax = sys_read(frame->ebx, frame->ecx, frame->edx);
    break;

  case SYS_WRITE:
    frame->eax = sys_write(frame->ebx, frame->ecx, frame->edx);
    break;

  case SYS_CLEAR:
    fb_clear();
    frame->eax = 0;
    break;

  case SYS_MEMINFO:
    frame->eax = sys_meminfo(frame->ebx);
    break;

  case SYS_OPEN:
    frame->eax = syscall_fs_open(frame->ebx, frame->ecx);
    break;

  case SYS_CLOSE:
    frame->eax = syscall_fs_close(frame->ebx);
    break;

  case SYS_READDIR:
    frame->eax = syscall_fs_readdir(frame->ebx, frame->ecx, frame->edx);
    break;

  case SYS_BRK:
    frame->eax = sys_brk(frame->ebx);
    break;

  case SYS_EXEC:
    frame->eax = sys_exec(frame->ebx);
    break;

  case SYS_SPAWN:
    frame->eax = sys_spawn(frame->ebx, frame->ecx);
    break;

  case SYS_WAIT:
    frame->eax = sys_wait(frame->ebx, frame->ecx);
    break;

  case SYS_LSEEK:
    frame->eax = syscall_fs_lseek(frame->ebx, (int32_t)frame->ecx, frame->edx);
    break;

  case SYS_STAT:
    frame->eax = syscall_fs_stat(frame->ebx, frame->ecx);
    break;

  case SYS_FSTAT:
    frame->eax = syscall_fs_fstat(frame->ebx, frame->ecx);
    break;

  case SYS_MKDIR:
    frame->eax = syscall_fs_mkdir(frame->ebx);
    break;

  case SYS_UNLINK:
    frame->eax = syscall_fs_unlink(frame->ebx);
    break;

  default:
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Unknown syscall: %u (eip=0x%x)",
                num, frame->eip);
    frame->eax = (uint32_t)-1;
    break;
  }
}
