#include "syscall.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "paging.h"
#include "serial.h"
#include "shell.h"

#define USER_SPACE_LIMIT 0xC0000000U

static int is_valid_user_buffer(uint32_t ptr, uint32_t len) {
  if (ptr == 0 || ptr >= USER_SPACE_LIMIT) {
    return 0;
  }
  if (len > USER_SPACE_LIMIT - ptr) {
    return 0;
  }
  return 1;
}

static uint32_t sys_read(uint32_t fd, uint32_t buf_addr, uint32_t len) {
  if (fd != 0 || len == 0) {
    return (uint32_t)-1;
  }

  if (!is_valid_user_buffer(buf_addr, len)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "sys_read: invalid user buffer 0x%x (len=%u)", buf_addr, len);
    return (uint32_t)-1;
  }

  char kbuf[128];
  unsigned int cap = len < sizeof(kbuf) ? len : sizeof(kbuf);

  /* Re-enable interrupts so IRQ1 (keyboard) can wake hlt inside keyboard_read_line */
  __asm__ volatile("sti");
  int count = keyboard_read_line(kbuf, cap);
  __asm__ volatile("cli");

  if (count < 0) {
    return (uint32_t)-1;
  }

  char *user_buf = (char *)buf_addr;
  for (int i = 0; i <= count; i++) {
    user_buf[i] = kbuf[i];
  }

  return (uint32_t)count;
}

static uint32_t sys_write(uint32_t fd, uint32_t buf_addr, uint32_t len) {
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

static uint32_t sys_meminfo(uint32_t info_addr) {
  if (!is_valid_user_buffer(info_addr, sizeof(struct user_meminfo))) {
    return (uint32_t)-1;
  }

  struct user_meminfo *info = (struct user_meminfo *)info_addr;
  info->free_frames = memory_get_free_frames();
  info->frame_size = 4096U;
  return 0;
}

static void sys_exit(int status) {
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "User program exited with status: %d", status);

  paging_switch_to_kernel_directory();
  __asm__ volatile("sti");
  shell_run();

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

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

    default:
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                  "Unknown syscall: %u (eip=0x%x)", num, frame->eip);
      frame->eax = (uint32_t)-1;
      break;
  }
}
