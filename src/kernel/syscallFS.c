#include "syscallFS.h"
#include "ff.h"
#include "log.h"
#include "syscall.h"
#include "task.h"

#define USER_SPACE_LIMIT 0xC0000000U
#define MAX_OPEN_FILES 16U

static FIL open_files[MAX_OPEN_FILES];
static uint8_t open_file_used[MAX_OPEN_FILES];
static int open_file_owner[MAX_OPEN_FILES];
static char open_file_path[MAX_OPEN_FILES][256];

static int is_valid_user_buffer(uint32_t ptr, uint32_t len) {
  if (ptr == 0 || ptr >= USER_SPACE_LIMIT) {
    return 0;
  }
  if (len > USER_SPACE_LIMIT - ptr) {
    return 0;
  }
  return 1;
}

static int is_valid_user_string(uint32_t ptr, uint32_t max_len) {
  if (ptr == 0 || ptr >= USER_SPACE_LIMIT) {
    return 0;
  }
  const char *s = (const char *)ptr;
  for (uint32_t i = 0; i < max_len; i++) {
    if (ptr + i >= USER_SPACE_LIMIT) {
      return 0;
    }
    if (s[i] == '\0') {
      return 1;
    }
  }
  return 0;
}

//-------------------------------------------------------------------------------------
// Canonicalises input against the calling task's cwd.  Segments are processed
// one at a time so "//", "." and ".." collapse without allocations; ".." is
// clamped at the root.  The output never has a trailing slash unless it is
// exactly "/".
//-------------------------------------------------------------------------------------
void fs_resolve_path(const char *input, char *out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }

  char combined[2 * TASK_CWD_MAX + 2];
  size_t pos = 0;

  if (!input) {
    input = "";
  }

  if (input[0] != '/') {
    struct task *task = task_current();
    const char *cwd = (task && task->cwd[0] == '/') ? task->cwd : "/";

    while (*cwd != '\0' && pos + 1U < sizeof(combined)) {
      combined[pos++] = *cwd++;
    }
    if (pos == 0 || combined[pos - 1U] != '/') {
      if (pos + 1U < sizeof(combined)) {
        combined[pos++] = '/';
      }
    }
  }

  while (*input != '\0' && pos + 1U < sizeof(combined)) {
    combined[pos++] = *input++;
  }
  combined[pos] = '\0';

  size_t out_len = 1;
  out[0] = '/';

  const char *p = combined;

  while (*p != '\0') {
    while (*p == '/') {
      p++;
    }
    if (*p == '\0') {
      break;
    }

    const char *segment = p;

    while (*p != '\0' && *p != '/') {
      p++;
    }

    size_t segment_len = (size_t)(p - segment);

    if (segment_len == 1U && segment[0] == '.') {
      continue;
    }

    if (segment_len == 2U && segment[0] == '.' && segment[1] == '.') {
      /* Pop the last segment (clamped at the root). */
      while (out_len > 1U && out[out_len - 1U] != '/') {
        out_len--;
      }
      if (out_len > 1U) {
        out_len--;
      }
      continue;
    }

    if (out_len > 1U) {
      if (out_len + 1U >= out_size) {
        break;
      }
      out[out_len++] = '/';
    }

    size_t copied = 0;

    while (copied < segment_len) {
      if (out_len + 1U >= out_size) {
        break;
      }
      out[out_len++] = segment[copied++];
    }

    if (copied < segment_len) {
      break; /* Truncated: keep the NUL terminator valid. */
    }
  }

  if (out_len >= out_size) {
    out_len = out_size - 1U;
  }

  out[out_len] = '\0';
}

//-------------------------------------------------------------------------------------
// Validates a user path string and resolves it against the caller's cwd.
//-------------------------------------------------------------------------------------
static int resolve_user_path(uint32_t path_addr, char *out, size_t out_size) {
  if (!is_valid_user_string(path_addr, TASK_CWD_MAX)) {
    return 0;
  }

  fs_resolve_path((const char *)path_addr, out, out_size);
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int is_root_path(const char *path) {
  return path[0] == '/' && path[1] == '\0';
}

uint32_t syscall_fs_open(uint32_t path_addr, uint32_t flags) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved))) {
    return (uint32_t)-1;
  }

  int slot = -1;
  for (uint32_t i = 0; i < MAX_OPEN_FILES; i++) {
    if (!open_file_used[i]) {
      slot = (int)i;
      break;
    }
  }
  if (slot < 0) {
    return (uint32_t)-1;
  }

  BYTE mode = 0;
  if (flags & O_RDONLY) {
    mode |= FA_READ;
  }
  if (flags & O_WRONLY) {
    mode |= FA_WRITE;
  }
  if (flags & O_TRUNC) {
    mode |= FA_CREATE_ALWAYS;
  } else if (flags & O_CREAT) {
    mode |= FA_OPEN_ALWAYS;
  }

  if (f_open(&open_files[slot], resolved, mode) != FR_OK) {
    return (uint32_t)-1;
  }

  if (flags & O_APPEND) {
    f_lseek(&open_files[slot], f_size(&open_files[slot]));
  }

  uint32_t i = 0;
  while (i + 1U < sizeof(open_file_path[slot]) && resolved[i] != '\0') {
    open_file_path[slot][i] = resolved[i];
    i++;
  }
  open_file_path[slot][i] = '\0';

  open_file_used[slot] = 1;
  open_file_owner[slot] = task_current_pid();
  return (uint32_t)slot + SYSCALL_FS_FD_OFFSET;
}

//-------------------------------------------------------------------------------------
// Returns the slot for fd if it is open and owned by the calling process.
//-------------------------------------------------------------------------------------
static int fs_slot_for(uint32_t fd) {
  if (fd < SYSCALL_FS_FD_OFFSET ||
      fd >= SYSCALL_FS_FD_OFFSET + MAX_OPEN_FILES) {
    return -1;
  }

  uint32_t slot = fd - SYSCALL_FS_FD_OFFSET;

  if (!open_file_used[slot]) {
    return -1;
  }

  int pid = task_current_pid();

  if (pid != 0 && open_file_owner[slot] != pid) {
    return -1;
  }

  return (int)slot;
}

uint32_t syscall_fs_close(uint32_t fd) {
  int slot = fs_slot_for(fd);

  if (slot < 0) {
    return (uint32_t)-1;
  }

  f_close(&open_files[slot]);
  open_file_used[slot] = 0;
  open_file_owner[slot] = 0;
  open_file_path[slot][0] = '\0';
  return 0;
}

uint32_t syscall_fs_read(uint32_t fd, uint32_t buf_addr, uint32_t len) {
  if (len == 0) {
    return 0;
  }
  if (!is_valid_user_buffer(buf_addr, len)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "syscall_fs_read: invalid buffer 0x%x (len=%u)", buf_addr, len);
    return (uint32_t)-1;
  }

  int slot = fs_slot_for(fd);

  if (slot < 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "syscall_fs_read: invalid fd %u",
                fd);
    return (uint32_t)-1;
  }

  UINT bytes_read = 0;
  FRESULT fr =
      f_read(&open_files[slot], (void *)buf_addr, len, &bytes_read);
  if (fr != FR_OK) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "syscall_fs_read: f_read failed (fr=%d, fd=%u, len=%u)", fr, fd,
                len);
    return (uint32_t)-1;
  }
  return (uint32_t)bytes_read;
}

uint32_t syscall_fs_write(uint32_t fd, uint32_t buf_addr, uint32_t len) {
  if (len == 0) {
    return 0;
  }
  if (!is_valid_user_buffer(buf_addr, len)) {
    return (uint32_t)-1;
  }

  int slot = fs_slot_for(fd);

  if (slot < 0) {
    return (uint32_t)-1;
  }

  UINT bytes_written = 0;
  if (f_write(&open_files[slot], (const void *)buf_addr, len, &bytes_written) !=
      FR_OK) {
    return (uint32_t)-1;
  }
  return (uint32_t)bytes_written;
}

uint32_t syscall_fs_lseek(uint32_t fd, int32_t offset, uint32_t whence) {
  int slot = fs_slot_for(fd);

  if (slot < 0) {
    return (uint32_t)-1;
  }

  FIL *fp = &open_files[slot];
  int64_t base;

  switch (whence) {
  case LSEEK_SET:
    base = 0;
    break;
  case LSEEK_CUR:
    base = (int64_t)f_tell(fp);
    break;
  case LSEEK_END:
    base = (int64_t)f_size(fp);
    break;
  default:
    return (uint32_t)-1;
  }

  int64_t target = base + (int64_t)offset;

  if (target < 0) {
    return (uint32_t)-1;
  }

  if (target > (int64_t)f_size(fp)) {
    target = (int64_t)f_size(fp);
  }

  if (f_lseek(fp, (FSIZE_t)target) != FR_OK) {
    return (uint32_t)-1;
  }

  return (uint32_t)f_tell(fp);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void fill_user_stat(const FILINFO *fno, struct user_stat *st) {
  st->size = (uint32_t)fno->fsize;
  st->date = (uint16_t)fno->fdate;
  st->time = (uint16_t)fno->ftime;
  st->is_dir = (fno->fattrib & AM_DIR) ? 1U : 0U;
  st->is_readonly = (fno->fattrib & AM_RDO) ? 1U : 0U;
  st->reserved = 0;
}

uint32_t syscall_fs_stat(uint32_t path_addr, uint32_t stat_addr) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved)) ||
      !is_valid_user_buffer(stat_addr, sizeof(struct user_stat))) {
    return (uint32_t)-1;
  }

  struct user_stat *st = (struct user_stat *)stat_addr;

  /*
   * FatFs reports the origin directory ("/") as success without filling in a
   * directory entry, so its attribute byte is not meaningful there.
   */
  if (is_root_path(resolved)) {
    st->size = 0;
    st->date = 0;
    st->time = 0;
    st->is_dir = 1;
    st->is_readonly = 0;
    st->reserved = 0;
    return 0;
  }

  FILINFO fno;

  if (f_stat(resolved, &fno) != FR_OK) {
    return (uint32_t)-1;
  }

  fill_user_stat(&fno, st);
  return 0;
}

uint32_t syscall_fs_fstat(uint32_t fd, uint32_t stat_addr) {
  if (!is_valid_user_buffer(stat_addr, sizeof(struct user_stat))) {
    return (uint32_t)-1;
  }

  int slot = fs_slot_for(fd);

  if (slot < 0) {
    return (uint32_t)-1;
  }

  FILINFO fno;

  if (open_file_path[slot][0] == '\0' ||
      f_stat(open_file_path[slot], &fno) != FR_OK) {
    return (uint32_t)-1;
  }

  fill_user_stat(&fno, (struct user_stat *)stat_addr);
  return 0;
}

uint32_t syscall_fs_mkdir(uint32_t path_addr) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved))) {
    return (uint32_t)-1;
  }

  return f_mkdir(resolved) == FR_OK ? 0 : (uint32_t)-1;
}

uint32_t syscall_fs_unlink(uint32_t path_addr) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved))) {
    return (uint32_t)-1;
  }

  return f_unlink(resolved) == FR_OK ? 0 : (uint32_t)-1;
}

uint32_t syscall_fs_readdir(uint32_t path_addr, uint32_t target_idx,
                            uint32_t dirent_addr) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved)) ||
      !is_valid_user_buffer(dirent_addr, sizeof(struct user_dirent))) {
    return (uint32_t)-1;
  }

  DIR dir;
  FILINFO fno;
  if (f_opendir(&dir, resolved) != FR_OK) {
    return (uint32_t)-1;
  }

  uint32_t idx = 0;
  uint32_t found = 0;

  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    if (idx == target_idx) {
      struct user_dirent *out = (struct user_dirent *)dirent_addr;
      uint32_t i = 0;
      while (i < sizeof(out->name) - 1U && fno.fname[i] != '\0') {
        out->name[i] = fno.fname[i];
        i++;
      }
      out->name[i] = '\0';
      out->size = (uint32_t)fno.fsize;
      out->is_dir = (fno.fattrib & AM_DIR) ? 1U : 0U;
      found = 1;
      break;
    }
    idx++;
  }

  f_closedir(&dir);
  return found;
}

uint32_t syscall_fs_chdir(uint32_t path_addr) {
  char resolved[TASK_CWD_MAX];

  if (!resolve_user_path(path_addr, resolved, sizeof(resolved))) {
    return (uint32_t)-1;
  }

  struct task *task = task_current();

  if (!task) {
    return (uint32_t)-1;
  }

  if (!is_root_path(resolved)) {
    FILINFO fno;

    if (f_stat(resolved, &fno) != FR_OK || !(fno.fattrib & AM_DIR)) {
      return (uint32_t)-1;
    }
  }

  size_t i = 0;
  while (i + 1U < TASK_CWD_MAX && resolved[i] != '\0') {
    task->cwd[i] = resolved[i];
    i++;
  }
  task->cwd[i] = '\0';

  return 0;
}

void syscall_fs_close_all(void) {
  for (uint32_t i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_used[i]) {
      f_close(&open_files[i]);
      open_file_used[i] = 0;
      open_file_owner[i] = 0;
      open_file_path[i][0] = '\0';
    }
  }
}

void syscall_fs_close_for_pid(int pid) {
  for (uint32_t i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_file_used[i] && open_file_owner[i] == pid) {
      f_close(&open_files[i]);
      open_file_used[i] = 0;
      open_file_owner[i] = 0;
      open_file_path[i][0] = '\0';
    }
  }
}
