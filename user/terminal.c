#include <stdint.h>

#define SYS_EXIT 1U
#define SYS_READ 3U
#define SYS_WRITE 4U
#define SYS_CLEAR 5U
#define SYS_MEMINFO 6U
#define SYS_OPEN 7U
#define SYS_CLOSE 8U
#define SYS_READDIR 9U
#define SYS_BRK     10U
#define SYS_EXEC    11U
#define SYS_SPAWN   12U
#define SYS_WAIT    13U

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

static inline int32_t syscall3(uint32_t num, uint32_t arg1, uint32_t arg2,
                               uint32_t arg3) {
  int32_t ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret), "+b"(arg1), "+c"(arg2), "+d"(arg3)
                   : "a"(num)
                   : "memory");
  return ret;
}

static uint32_t str_len(const char *s) {
  uint32_t len = 0;
  while (s[len] != '\0') {
    len++;
  }
  return len;
}

static int str_eq(const char *a, const char *b) {
  uint32_t i = 0;
  while (a[i] != '\0' && b[i] != '\0') {
    if (a[i] != b[i]) {
      return 0;
    }
    i++;
  }
  return a[i] == b[i];
}

static int starts_with(const char *s, const char *prefix) {
  uint32_t i = 0;
  while (prefix[i] != '\0') {
    if (s[i] != prefix[i]) {
      return 0;
    }
    i++;
  }
  return 1;
}

static void write_str(const char *s) {
  uint32_t len = str_len(s);
  if (len > 0) {
    syscall3(SYS_WRITE, 1U, (uint32_t)(uintptr_t)s, len);
  }
}

static void write_uint(uint32_t val) {
  char buf[16];
  int i = 0;
  if (val == 0) {
    write_str("0");
    return;
  }
  while (val > 0 && i < 15) {
    buf[i++] = (char)('0' + (val % 10U));
    val /= 10U;
  }
  while (i > 0) {
    char c = buf[--i];
    syscall3(SYS_WRITE, 1U, (uint32_t)(uintptr_t)&c, 1U);
  }
}

static void cmd_ls(const char *path) {
  struct user_dirent ent;
  uint32_t idx = 0;

  while (idx < 256U) {
    int32_t res = syscall3(SYS_READDIR, (uint32_t)(uintptr_t)path, idx,
                           (uint32_t)(uintptr_t)&ent);

    write_str("res=");
    write_uint((uint32_t)res);
    write_str(" ");

    if (res < 0) {
      write_str("ls: cannot open directory\n");
      return;
    }
    if (res == 0) {
      if (idx == 0) {
        write_str("(empty)\n");
      }
      return;
    }

    write_str(ent.is_dir ? "[DIR]  " : "[FILE] ");
    write_str(ent.name);
    if (!ent.is_dir) {
      write_str(" (");
      write_uint(ent.size);
      write_str(" B)");
    }
    idx++;
    write_str("\n");
  }
}

static void cmd_cat(const char *path) {
  int32_t fd = syscall3(SYS_OPEN, (uint32_t)(uintptr_t)path, O_RDONLY, 0U);
  if (fd < 0) {
    write_str("cat: file not found\n");
    return;
  }

  char buf[128];
  int32_t bytes;
  while ((bytes = syscall3(SYS_READ, (uint32_t)fd, (uint32_t)(uintptr_t)buf,
                           sizeof(buf))) > 0) {
    syscall3(SYS_WRITE, 1U, (uint32_t)(uintptr_t)buf, (uint32_t)bytes);
  }
  if (bytes < 0) {
    write_str("cat: read error\n");
  }

  syscall3(SYS_CLOSE, (uint32_t)fd, 0U, 0U);
}

static void cmd_write(char *args) {
  char *space = args;
  while (*space != '\0' && *space != ' ') {
    space++;
  }
  if (*space == '\0') {
    write_str("Usage: write <path> <text>\n");
    return;
  }

  *space = '\0';
  const char *text = space + 1;

  int32_t fd = syscall3(SYS_OPEN, (uint32_t)(uintptr_t)args,
                        O_WRONLY | O_CREAT | O_TRUNC, 0U);
  if (fd < 0) {
    write_str("write: failed to open file\n");
    return;
  }

  uint32_t len = str_len(text);
  if (len > 0) {
    syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)(uintptr_t)text, len);
  }
  syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)(uintptr_t)"\n", 1U);
  syscall3(SYS_CLOSE, (uint32_t)fd, 0U, 0U);
  write_str("Saved.\n");
}

void _start(void) {
  char line[128];

  write_str("========================================\n");
  write_str("  DragoonOS Ring 3 User-Space Terminal  \n");
  write_str("========================================\n");
  write_str("Type 'help' for available commands.\n\n");

  while (1) {
    write_str("dragoon-user> ");
    int32_t n =
        syscall3(SYS_READ, 0U, (uint32_t)(uintptr_t)line, sizeof(line) - 1U);
    if (n <= 0) {
      continue;
    }
    line[n] = '\0';

    if (str_eq(line, "help")) {
      write_str("Commands:\n");
      write_str("  help               - Show this help\n");
      write_str("  clear              - Clear screen\n");
      write_str("  meminfo            - Show free physical memory\n");
      write_str("  ls [path]          - List directory contents\n");
      write_str("  cat <path>         - Print file contents\n");
      write_str("  write <path> <txt> - Create or overwrite file\n");
      write_str("  run <path>         - Run ELF binary as a child process\n");
      write_str("  exit               - Exit to kernel shell\n");
    } else if (str_eq(line, "clear")) {
      syscall3(SYS_CLEAR, 0U, 0U, 0U);
    } else if (str_eq(line, "meminfo")) {
      struct user_meminfo info;
      if (syscall3(SYS_MEMINFO, (uint32_t)(uintptr_t)&info, 0U, 0U) == 0) {
        write_str("Free frames: ");
        write_uint(info.free_frames);
        write_str(" (");
        write_uint((info.free_frames * info.frame_size) / 1024U);
        write_str(" KB)\n");
      }
    } else if (str_eq(line, "ls")) {
      cmd_ls("/");
    } else if (starts_with(line, "ls ")) {
      cmd_ls(line + 3);
    } else if (starts_with(line, "cat ")) {
      cmd_cat(line + 4);
    } else if (starts_with(line, "write ")) {
      cmd_write(line + 6);
    } else if (str_eq(line, "exit")) {
      syscall3(SYS_EXIT, 0U, 0U, 0U);

    } else if (starts_with(line, "run ")) {
      int32_t pid = syscall3(SYS_SPAWN, (uint32_t)(uintptr_t)(line + 4), 0U, 0U);

      if (pid < 0) {
        write_str("run: failed to execute ELF binary\n");
      } else {
        int32_t status = -1;
        int32_t waited =
            syscall3(SYS_WAIT, (uint32_t)pid, (uint32_t)(uintptr_t)&status, 0U);

        if (waited < 0) {
          write_str("run: wait failed\n");
        } else {
          write_str("run: child exited with status ");
          write_uint((uint32_t)status);
          write_str("\n");
        }
      }
    }
  }
}
