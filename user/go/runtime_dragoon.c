#include <stddef.h>
#include <stdint.h>

#define SYS_EXIT 1U
#define SYS_READ 3U
#define SYS_WRITE 4U
#define SYS_OPEN 7U
#define SYS_CLOSE 8U
#define SYS_READDIR 9U
#define SYS_BRK 10U
#define SYS_LSEEK 14U
#define SYS_STAT 15U
#define SYS_FSTAT 16U
#define SYS_MKDIR 17U
#define SYS_UNLINK 18U
#define SYS_CHDIR 19U
#define SYS_GETCWD 20U
#define SYS_SPAWN 12U
#define SYS_WAIT 13U
#define SYS_TTY_MODE 21U

/* Status record layout shared with include/syscall.h. */
struct dragoon_stat {
  uint32_t size;
  uint16_t date;
  uint16_t time;
  uint8_t is_dir;
  uint8_t is_readonly;
  uint16_t reserved;
};

/* Directory entry layout shared with include/syscall.h. */
struct dragoon_dirent {
  char name[64];
  uint32_t size;
  uint8_t is_dir;
};

extern int main(int argc, char **argv);

/*
 * The Go runtime queries these through //go:linkname.  Keeping the storage in
 * C makes it opaque to the Go linker's whole-program optimiser, which would
 * otherwise prove the values are still zero when package initialisers run.
 */
int dragoon_argc = 0;
char **dragoon_argv = 0;

void dragoon_set_args(int argc, char **argv) {
  dragoon_argc = argc;
  dragoon_argv = argv;
}

int dragoon_get_argc(void) { return dragoon_argc; }

char **dragoon_get_argv(void) { return dragoon_argv; }

static inline int32_t dragoon_syscall3(uint32_t num, uint32_t a1, uint32_t a2,
                                       uint32_t a3) {
  int32_t ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret), "+b"(a1), "+c"(a2), "+d"(a3)
                   : "a"(num)
                   : "memory");
  return ret;
}

void _exit(int status) {
  dragoon_syscall3(SYS_EXIT, (uint32_t)status, 0U, 0U);
  for (;;) {
  }
}

void abort(void) { _exit(1); }

void *sbrk(intptr_t increment) {
  uint32_t cur = (uint32_t)dragoon_syscall3(SYS_BRK, 0U, 0U, 0U);
  if (increment == 0) {
    return (void *)(uintptr_t)cur;
  }
  uint32_t target = (uint32_t)((int32_t)cur + (int32_t)increment);
  uint32_t res = (uint32_t)dragoon_syscall3(SYS_BRK, target, 0U, 0U);
  if (res != target) {
    return (void *)(uintptr_t)-1;
  }
  return (void *)(uintptr_t)cur;
}

int write(int fd, const void *buf, size_t count) {
  return (int)dragoon_syscall3(SYS_WRITE, (uint32_t)fd,
                               (uint32_t)(uintptr_t)buf, (uint32_t)count);
}

/*
 * Filesystem wrappers.  Flags use the kernel's O_* bits
 * (include/syscall.h); the TinyGo os layer translates Go's syscall flags.
 */
int open(const char *path, int flags) {
  return (int)dragoon_syscall3(SYS_OPEN, (uint32_t)(uintptr_t)path,
                               (uint32_t)flags, 0U);
}

int read(int fd, void *buf, size_t count) {
  return (int)dragoon_syscall3(SYS_READ, (uint32_t)fd, (uint32_t)(uintptr_t)buf,
                               (uint32_t)count);
}

int close(int fd) {
  return (int)dragoon_syscall3(SYS_CLOSE, (uint32_t)fd, 0U, 0U);
}

int lseek(int fd, int offset, int whence) {
  return (int)dragoon_syscall3(SYS_LSEEK, (uint32_t)fd, (uint32_t)offset,
                               (uint32_t)whence);
}

int stat(const char *path, struct dragoon_stat *st) {
  return (int)dragoon_syscall3(SYS_STAT, (uint32_t)(uintptr_t)path,
                               (uint32_t)(uintptr_t)st, 0U);
}

int fstat(int fd, struct dragoon_stat *st) {
  return (int)dragoon_syscall3(SYS_FSTAT, (uint32_t)fd, (uint32_t)(uintptr_t)st,
                               0U);
}

int mkdir(const char *path) {
  return (int)dragoon_syscall3(SYS_MKDIR, (uint32_t)(uintptr_t)path, 0U, 0U);
}

int unlink(const char *path) {
  return (int)dragoon_syscall3(SYS_UNLINK, (uint32_t)(uintptr_t)path, 0U, 0U);
}

int readdir(const char *path, int index, struct dragoon_dirent *out) {
  return (int)dragoon_syscall3(SYS_READDIR, (uint32_t)(uintptr_t)path,
                               (uint32_t)index, (uint32_t)(uintptr_t)out);
}

/*
 * Working-directory wrappers.  Relative paths passed to open/stat/mkdir/
 * unlink/readdir are resolved by the kernel against the calling task's cwd,
 * which chdir updates.
 */
int chdir(const char *path) {
  return (int)dragoon_syscall3(SYS_CHDIR, (uint32_t)(uintptr_t)path, 0U, 0U);
}

char *getcwd(char *buf, size_t size) {
  int32_t res = dragoon_syscall3(SYS_GETCWD, (uint32_t)(uintptr_t)buf,
                                 (uint32_t)size, 0U);

  if (res < 0) {
    return (char *)0;
  }

  return buf;
}

int putchar(int c) {
  char ch = (char)c;
  write(1, &ch, 1);
  return c;
}

void *memcpy(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++)
    d[i] = s[i];
  return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d < s) {
    for (size_t i = 0; i < n; i++)
      d[i] = s[i];
  } else if (d > s) {
    for (size_t i = n; i > 0; i--)
      d[i - 1] = s[i - 1];
  }
  return dst;
}

void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;
  for (size_t i = 0; i < n; i++)
    p[i] = (uint8_t)c;
  return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *a = (const uint8_t *)s1;
  const uint8_t *b = (const uint8_t *)s2;
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i])
      return (int)a[i] - (int)b[i];
  }
  return 0;
}

void _start(int argc, char **argv) {
  int code = main(argc, argv);
  _exit(code);
}

int dragoon_tty_raw(int enable) {
  return (int)dragoon_syscall3(SYS_TTY_MODE, (uint32_t)(enable != 0), 0U, 0U);
}

int dragoon_spawn(const char *path, const char *const *argv) {
  return (int)dragoon_syscall3(SYS_SPAWN, (uint32_t)(uintptr_t)path,
                               (uint32_t)(uintptr_t)argv, 0U);
}

int dragoon_wait(int pid, int *status) {
  return (int)dragoon_syscall3(SYS_WAIT, (uint32_t)pid,
                               (uint32_t)(uintptr_t)status, 0U);
}
