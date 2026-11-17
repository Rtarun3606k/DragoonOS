#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 32
#define TASK_KSTACK_SIZE 8192U

/* Command-line limits shared by the syscall layer and the spawn path. */
#define MAX_ARGS 16
#define MAX_ARG_LEN 256


#define TASK_CWD_MAX 256

enum task_state {
  TASK_UNUSED = 0,
  TASK_READY,
  TASK_RUNNING,
  TASK_WAITING,
  TASK_ZOMBIE,
};

/*
 * One Ring 3 process.  Every task owns a private user page directory (cr3)
 * plus a kernel stack used for Ring 3 -> Ring 0 transitions and for blocking
 * syscalls.  The kernel half of every page directory is shared.
 */
struct task {
  int pid;
  int parent_pid;
  enum task_state state;
  int exit_code;
  int wait_pid;   /* pid this task is blocked on (TASK_WAITING) */
  int wait_stdin; /* nonzero when blocked in SYS_READ on fd 0 */

  uint32_t cr3; /* physical user page directory */
  uint32_t esp; /* saved kernel stack pointer while not running */

  void *kernel_stack;
  uint32_t kernel_stack_top;

  uint32_t entry; /* user entry point */
  uint32_t user_stack_top;

  uint32_t brk_min;
  uint32_t brk_cur;
  char cwd[TASK_CWD_MAX];
};

struct task *task_current(void);
int task_current_pid(void);

/* Creates a task from an in-memory ELF image (used for the boot module). */
int task_create_from_image(const void *image, unsigned int size, int parent_pid,
                           struct task **out);

/*
 * Creates a child of the current task, loading path from the FAT filesystem.
 * argv is a NULL-terminated array of NUL-terminated strings; passing NULL
 * defaults to { path, NULL }.  argc/argv are placed on the child's user stack.
 */
int task_spawn(const char *path, const char *const *argv, struct task **out);

/* Enters the first Ring 3 task; never returns. */
void task_start_first(struct task *task) __attribute__((noreturn));

/* Round-robin scheduler.  Returns when the calling task is scheduled again. */
void task_schedule(void);

/* Called from the PIT IRQ with the interrupted code segment. */
void task_timer_tick(uint32_t interrupted_cs);

/* Terminates the current task, wakes its parent and yields.  Never returns. */
void task_exit_current(int status) __attribute__((noreturn));

/* Blocks until a child exits, reaps it and returns its pid (or -1). */
int task_wait_pid(int pid, int *exit_code_out);

/* Called from the keyboard IRQ after stdin data/EOF becomes available. */
void task_wake_stdin(void);

#endif
