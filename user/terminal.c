#define SYS_EXIT    1
#define SYS_READ    3
#define SYS_WRITE   4
#define SYS_CLEAR   5
#define SYS_MEMINFO 6

struct meminfo {
  unsigned int free_frames;
  unsigned int frame_size;
};

static inline int syscall3(int num, int arg1, int arg2, int arg3) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
                   : "memory");
  return ret;
}

static void write_str(const char *s) {
  unsigned int len = 0;
  while (s[len]) len++;
  syscall3(SYS_WRITE, 1, (int)s, (int)len);
}

static void write_uint(unsigned int val) {
  char buf[16];
  int i = 0;
  if (val == 0) {
    write_str("0");
    return;
  }
  while (val > 0) {
    buf[i++] = (char)('0' + (val % 10));
    val /= 10;
  }
  while (i > 0) {
    i--;
    syscall3(SYS_WRITE, 1, (int)&buf[i], 1);
  }
}

static int read_line(char *buf, unsigned int max_len) {
  return syscall3(SYS_READ, 0, (int)buf, (int)max_len);
}

static void clear_screen(void) {
  syscall3(SYS_CLEAR, 0, 0, 0);
}

static void exit_program(int code) {
  syscall3(SYS_EXIT, code, 0, 0);
}

static int str_eq(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b) return 0;
    a++;
    b++;
  }
  return *a == *b;
}

static int starts_with(const char *str, const char *prefix) {
  while (*prefix) {
    if (*str != *prefix) return 0;
    str++;
    prefix++;
  }
  return 1;
}

void _start(void) {
  char line[128];

  clear_screen();
  write_str("========================================\n");
  write_str("  DragoonOS Ring 3 User-Space Terminal  \n");
  write_str("========================================\n");
  write_str("Type 'help' for available commands.\n\n");

  while (1) {
    write_str("dragoon-user> ");
    int n = read_line(line, sizeof(line));
    if (n <= 0) {
      continue;
    }

    if (str_eq(line, "help")) {
      write_str("Commands:\n");
      write_str("  help        - Show this message\n");
      write_str("  echo <text> - Print text via sys_write\n");
      write_str("  meminfo     - Query free RAM via sys_meminfo\n");
      write_str("  clear       - Clear screen via sys_clear\n");
      write_str("  exit        - Exit Ring 3 via sys_exit\n");
    } else if (starts_with(line, "echo ")) {
      write_str(line + 5);
      write_str("\n");
    } else if (str_eq(line, "clear")) {
      clear_screen();
    } else if (str_eq(line, "meminfo")) {
      struct meminfo info;
      if (syscall3(SYS_MEMINFO, (int)&info, 0, 0) == 0) {
        write_str("Free frames: ");
        write_uint(info.free_frames);
        write_str(" (");
        write_uint((info.free_frames * info.frame_size) / 1024);
        write_str(" KiB free)\n");
      }
    } else if (str_eq(line, "exit")) {
      write_str("Exiting user terminal...\n");
      exit_program(0);
    } else {
      write_str("Unknown command: ");
      write_str(line);
      write_str("\n");
    }
  }
}
