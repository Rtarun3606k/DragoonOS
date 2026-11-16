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

#define MAX_TERM_ARGS 16

struct user_meminfo {
  unsigned int free_frames;
  unsigned int frame_size;
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

//-------------------------------------------------------------------------------------
// Splits buf in place on spaces/tabs.  argv is NULL-terminated.
//-------------------------------------------------------------------------------------
static int tokenize(char *buf, char *argv[], int max_args) {
  int argc = 0;
  char *p = buf;

  while (*p != '\0') {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
      p++;
    }
    if (*p == '\0') {
      break;
    }
    if (argc >= max_args) {
      break;
    }

    argv[argc++] = p;

    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' &&
           *p != '\r') {
      p++;
    }
    if (*p != '\0') {
      *p++ = '\0';
    }
  }

  argv[argc] = 0;
  return argc;
}

//-------------------------------------------------------------------------------------
// Builds "/bin/<cmd>.bin" into out.
//-------------------------------------------------------------------------------------
static int build_bin_path(const char *cmd, char *out, uint32_t capacity) {
  const char *prefix = "/bin/";
  const char *suffix = ".bin";
  uint32_t len = 0;

  while (*prefix != '\0') {
    if (len + 1U >= capacity) {
      return 0;
    }
    out[len++] = *prefix++;
  }
  while (*cmd != '\0') {
    if (len + 1U >= capacity) {
      return 0;
    }
    out[len++] = *cmd++;
  }
  while (*suffix != '\0') {
    if (len + 1U >= capacity) {
      return 0;
    }
    out[len++] = *suffix++;
  }

  out[len] = '\0';
  return 1;
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

  while (len > 0 && (text[len - 1U] == '\n' || text[len - 1U] == '\r')) {
    len--;
  }

  if (len > 0) {
    syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)(uintptr_t)text, len);
  }
  syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)(uintptr_t)"\n", 1U);
  syscall3(SYS_CLOSE, (uint32_t)fd, 0U, 0U);
  write_str("Saved.\n");
}

//-------------------------------------------------------------------------------------
// Spawns path with the given argv, waits for it and optionally reports the
// exit status.  report_status selects the "run" wording on failure.
//-------------------------------------------------------------------------------------
static int32_t run_program(uint32_t path, char *const *argv, int report_status) {
  int32_t pid = syscall3(SYS_SPAWN, path, (uint32_t)(uintptr_t)argv, 0U);

  if (pid < 0) {
    if (report_status) {
      write_str("run: failed to execute ELF binary\n");
    } else {
      write_str("unknown command: ");
      write_str(argv[0]);
      write_str("\n");
    }
    return -1;
  }

  int32_t status = -1;
  int32_t waited =
      syscall3(SYS_WAIT, (uint32_t)pid, (uint32_t)(uintptr_t)&status, 0U);

  if (report_status) {
    if (waited < 0) {
      write_str("run: wait failed\n");
    } else {
      write_str("run: child exited with status ");
      write_uint((uint32_t)status);
      write_str("\n");
    }
  }

  return waited;
}

void _start(void) {
  char line[128];
  char work[128];
  char resolved[160];
  char *argv[MAX_TERM_ARGS + 1];

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

    /* Tokenise a copy so `write` can still see the original spacing. */
    for (int32_t i = 0; i <= n; i++) {
      work[i] = line[i];
    }

    int argc = tokenize(work, argv, MAX_TERM_ARGS);
    if (argc == 0) {
      continue;
    }

    if (str_eq(argv[0], "help")) {
      write_str("Commands:\n");
      write_str("  help               - Show this help\n");
      write_str("  clear              - Clear screen\n");
      write_str("  meminfo            - Show free physical memory\n");
      write_str("  write <path> <txt> - Create or overwrite file\n");
      write_str("  ls [path]          - List directory (runs /bin/ls.bin)\n");
      write_str("  cat [path...]      - Print files or stdin (/bin/cat.bin)\n");
      write_str("  run <path> [args]  - Run ELF with arguments\n");
      write_str("  <cmd> [args]       - Run /bin/<cmd>.bin\n");
      write_str("  exit               - Exit to kernel shell\n");
    } else if (str_eq(argv[0], "clear")) {
      syscall3(SYS_CLEAR, 0U, 0U, 0U);
    } else if (str_eq(argv[0], "meminfo")) {
      struct user_meminfo info;
      if (syscall3(SYS_MEMINFO, (uint32_t)(uintptr_t)&info, 0U, 0U) == 0) {
        write_str("Free frames: ");
        write_uint(info.free_frames);
        write_str(" (");
        write_uint((info.free_frames * info.frame_size) / 1024U);
        write_str(" KB)\n");
      }
    } else if (str_eq(argv[0], "write")) {
      if (argc < 3) {
        write_str("Usage: write <path> <text>\n");
      } else {
        cmd_write(line + 6);
      }
    } else if (str_eq(argv[0], "exit")) {
      syscall3(SYS_EXIT, 0U, 0U, 0U);
    } else if (str_eq(argv[0], "run")) {
      if (argc < 2) {
        write_str("Usage: run <path> [args...]\n");
      } else {
        run_program((uint32_t)(uintptr_t)argv[1], &argv[1], 1);
      }
    } else if (starts_with(argv[0], "/")) {
      run_program((uint32_t)(uintptr_t)argv[0], argv, 0);
    } else if (build_bin_path(argv[0], resolved, sizeof(resolved))) {
      run_program((uint32_t)(uintptr_t)resolved, argv, 0);
    } else {
      write_str("unknown command: ");
      write_str(argv[0]);
      write_str("\n");
    }
  }
}
