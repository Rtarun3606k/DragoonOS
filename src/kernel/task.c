#include "task.h"
#include "elf.h"
#include "gdt.h"
#include "heap.h"
#include "log.h"
#include "memory.h"
#include "paging.h"
#include "shell.h"
#include "syscallFS.h"

#define USER_BRK_DEFAULT 0x08100000U

static struct task tasks[MAX_TASKS];
static struct task *current_task = 0;

extern void task_switch(uint32_t *old_esp, uint32_t new_esp);
static void task_bootstrap(void);
static void task_build_initial_context(struct task *task);
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void zero_task(struct task *task) {
  uint8_t *bytes = (uint8_t *)task;

  for (unsigned int i = 0; i < sizeof(*task); i++) {
    bytes[i] = 0;
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t read_cr3(void) {
  uint32_t value;

  __asm__ volatile("mov %%cr3, %0" : "=r"(value));

  return value;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
struct task *task_current(void) { return current_task; }

int task_current_pid(void) { return current_task ? current_task->pid : 0; }

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static struct task *task_find(int pid) {
  if (pid <= 0) {
    return 0;
  }

  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid) {
      return &tasks[i];
    }
  }

  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static struct task *task_alloc_slot(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_UNUSED) {
      zero_task(&tasks[i]);
      tasks[i].pid = i + 1;
      return &tasks[i];
    }
  }

  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void task_build_initial_context(struct task *task) {
  uint32_t *stack = (uint32_t *)task->kernel_stack_top;

  /* Context restored by task_switch: edi, esi, ebx, ebp, return address. */
  *--stack = (uint32_t)task_bootstrap;
  *--stack = 0; /* ebp */
  *--stack = 0; /* ebx */
  *--stack = 0; /* esi */
  *--stack = 0; /* edi */

  task->esp = (uint32_t)stack;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int task_setup_payload(struct task *task,
                              const struct elf_load_info *info) {
  task->entry = info->entry;
  task->user_stack_top = info->stack_top;

  task->brk_min = (info->brk_start + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
  if (task->brk_min < USER_BRK_DEFAULT) {
    task->brk_min = USER_BRK_DEFAULT;
  }
  task->brk_cur = task->brk_min;

  task->kernel_stack = kmalloc(TASK_KSTACK_SIZE);
  if (!task->kernel_stack) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Task: failed to allocate kernel stack");
    return 0;
  }
  task->kernel_stack_top = (uint32_t)task->kernel_stack + TASK_KSTACK_SIZE;

  task_build_initial_context(task);
  task->state = TASK_READY;
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t arg_length(const char *arg) {
  uint32_t len = 1;

  while (arg[len - 1U] != '\0') {
    len++;
  }

  return len;
}

//-------------------------------------------------------------------------------------
// Copies argv onto the child's user stack and builds the standard SysV i386
// cdecl entry frame:
//     [esp]     = 0        (dummy return address)
//     [esp + 4] = argc
//     [esp + 8] = argv
// The strings sit at the top of the stack, followed by the pointer array.
// Must be called with the child's page directory active.
//-------------------------------------------------------------------------------------
static int task_build_user_argv(struct task *task, const char *const *argv,
                                int argc) {
  uint32_t sp = task->user_stack_top & ~0xFU;
  uint32_t stack_low = USER_STACK_TOP - USER_STACK_SIZE;
  uint32_t arg_ptrs[MAX_ARGS];

  if (sp < stack_low) {
    return 0;
  }

  for (int i = argc - 1; i >= 0; i--) {
    uint32_t len = arg_length(argv[i]);

    if (sp < stack_low + len) {
      return 0;
    }

    sp -= len;

    char *dst = (char *)sp;

    for (uint32_t j = 0; j < len; j++) {
      dst[j] = argv[i][j];
    }

    arg_ptrs[i] = sp;
  }

  sp &= ~0xFU;
  sp -= (uint32_t)(argc + 1) * sizeof(uint32_t);

  if (sp < stack_low) {
    return 0;
  }

  uint32_t *user_argv = (uint32_t *)sp;

  for (int i = 0; i < argc; i++) {
    user_argv[i] = arg_ptrs[i];
  }
  user_argv[argc] = 0;

  uint32_t argv_address = sp;
  uint32_t frame = (sp - 12U) & ~0xFU;

  if (frame < stack_low) {
    return 0;
  }

  uint32_t *entry = (uint32_t *)frame;

  entry[0] = 0; /* dummy return address */
  entry[1] = (uint32_t)argc;
  entry[2] = argv_address;

  task->user_stack_top = frame;
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int task_apply_argv(struct task *task, const char *const *argv) {
  int argc = 0;

  while (argc < MAX_ARGS && argv[argc] != 0) {
    argc++;
  }

  uint32_t previous = paging_current_directory();

  paging_switch_directory(task->cr3);
  int ok = task_build_user_argv(task, argv, argc);
  paging_switch_directory(previous);

  return ok;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void task_free_slot(struct task *task) {
  if (task->kernel_stack) {
    kfree(task->kernel_stack);
  }
  if (task->cr3) {
    paging_destroy_user_directory(task->cr3);
  }
  task->state = TASK_UNUSED;
  task->cr3 = 0;
  task->kernel_stack = 0;
  task->kernel_stack_top = 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
int task_create_from_image(const void *image, unsigned int size, int parent_pid,
                           struct task **out) {
  if (!image || size == 0 || !out) {
    return 0;
  }

  struct task *task = task_alloc_slot();
  if (!task) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Task: no free task slots");
    return 0;
  }

  task->parent_pid = parent_pid;
  task->cwd[0] = '/';
  task->cwd[1] = '\0';

  task->cr3 = paging_create_user_directory();
  if (!task->cr3) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Task: failed to create user page directory");
    task->state = TASK_UNUSED;
    return 0;
  }

  struct elf_load_info info;
  if (!elf_load_image_into(task->cr3, image, size, &info)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Task: ELF load failed");
    task_free_slot(task);
    return 0;
  }

  if (!task_setup_payload(task, &info)) {
    task_free_slot(task);
    return 0;
  }

  static const char *const init_argv[] = {"/init", 0};

  if (!task_apply_argv(task, init_argv)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Task: failed to build argv for init");
    task_free_slot(task);
    return 0;
  }

  *out = task;
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
int task_spawn(const char *path, const char *const *argv, struct task **out) {
  const char *default_argv[2];
  char resolved[TASK_CWD_MAX];

  if (!path || !out || !current_task) {
    return 0;
  }

  if (!argv) {
    default_argv[0] = path;
    default_argv[1] = 0;
    argv = default_argv;
  }

  fs_resolve_path(path, resolved, sizeof(resolved));

  struct task *task = task_alloc_slot();
  if (!task) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Task: no free task slots");
    return 0;
  }

  task->parent_pid = current_task->pid;

  /* Children inherit the parent's working directory. */
  {
    uint32_t i = 0;

    while (i + 1U < TASK_CWD_MAX && current_task->cwd[i] != '\0') {
      task->cwd[i] = current_task->cwd[i];
      i++;
    }
    task->cwd[i] = '\0';

    if (task->cwd[0] != '/') {
      task->cwd[0] = '/';
      task->cwd[1] = '\0';
    }
  }

  task->cr3 = paging_create_user_directory();
  if (!task->cr3) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Task: failed to create user page directory");
    task->state = TASK_UNUSED;
    return 0;
  }

  struct elf_load_info info;
  if (!elf_load_file_into(task->cr3, resolved, &info)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Task: cannot load %s", resolved);
    task_free_slot(task);
    return 0;
  }

  if (!task_setup_payload(task, &info)) {
    task_free_slot(task);
    return 0;
  }

  if (!task_apply_argv(task, argv)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Task: failed to build argv for %s", resolved);
    task_free_slot(task);
    return 0;
  }

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Task: spawned pid %d (%s) from pid %d", task->pid, resolved,
              current_task->pid);

  *out = task;
  return 1;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void task_bootstrap(void) {
  struct task *task = current_task;

  if (!task) {
    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Task %d entering Ring 3 at 0x%x (parent %d)", task->pid,
              task->entry, task->parent_pid);

  enter_usermode(task->entry, task->user_stack_top);

  task_exit_current(-1);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void task_start_first(struct task *task) {
  if (!task) {
    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  task->state = TASK_RUNNING;
  current_task = task;

  paging_switch_directory(task->cr3);
  tss_set_stack(task->kernel_stack_top);

  uint32_t unused_esp = 0;
  task_switch(&unused_esp, task->esp);

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

//-------------------------------------------------------------------------------------
// Called from the keyboard IRQ when stdin bytes or an EOF marker arrive.
// Waking every stdin waiter lets each re-check the shared queue; the first one
// to run drains it and the rest park again.
//-------------------------------------------------------------------------------------
void task_wake_stdin(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_WAITING && tasks[i].wait_stdin) {
      tasks[i].wait_stdin = 0;
      tasks[i].state = TASK_READY;
    }
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static int task_any_ready(void) {
  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_READY) {
      return 1;
    }
  }

  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static struct task *task_pick_next(struct task *previous) {
  int start = 0;

  if (previous) {
    start = (int)(previous - tasks);
  }

  for (int offset = 1; offset <= MAX_TASKS; offset++) {
    int index = (start + offset) % MAX_TASKS;

    if (tasks[index].state == TASK_READY) {
      return &tasks[index];
    }
  }

  return 0;
}

//-------------------------------------------------------------------------------------
// Round-robin context switch.  Called from blocking syscalls and from the PIT
// IRQ; the incoming task's kernel stack resumes its own call chain.
//-------------------------------------------------------------------------------------
void task_schedule(void) {
  struct task *previous = current_task;

  if (!previous) {
    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  if (previous->state == TASK_RUNNING) {
    previous->state = TASK_READY;
  }

  for (;;) {
    struct task *next = task_pick_next(previous);

    if (next) {
      if (next == previous) {
        next->state = TASK_RUNNING;
        return;
      }

      next->state = TASK_RUNNING;
      current_task = next;

      if (read_cr3() != next->cr3) {
        paging_switch_directory(next->cr3);
      }
      tss_set_stack(next->kernel_stack_top);

      task_switch(&previous->esp, next->esp);
      return;
    }

    /*
     * No runnable task.  Park in the idle loop until an IRQ (keyboard input,
     * child exit, timer) makes one ready.  STI has a one-instruction shadow,
     * so `sti; hlt` is atomic with respect to the check above.
     */
    __asm__ volatile("sti; hlt; cli");
  }
}

//-------------------------------------------------------------------------------------
// Preemptive round robin: only switch when the interrupted context was Ring 3.
// A Ring 0 interruption may hold kernel locks/FAT state, so it is left alone.
//-------------------------------------------------------------------------------------
void task_timer_tick(uint32_t interrupted_cs) {
  if (!current_task) {
    return;
  }

  if ((interrupted_cs & 0x3U) != 0x3U) {
    return;
  }

  if (!task_any_ready()) {
    return;
  }

  task_schedule();
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void task_exit_current(int status) {
  struct task *task = current_task;

  if (!task) {
    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  task->exit_code = status;

  if (task->parent_pid != 0) {
    struct task *parent = task_find(task->parent_pid);

    if (parent && parent->state == TASK_WAITING &&
        (parent->wait_pid == task->pid || parent->wait_pid == -1)) {
      parent->state = TASK_READY;
    }

    task->state = TASK_ZOMBIE;
    task_schedule();

    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  /*
   * The init task has no parent.  Tear its address space down and fall back
   * to the kernel shell, preserving the historical "exit" command behaviour.
   */
  syscall_fs_close_for_pid(task->pid);
  task->state = TASK_ZOMBIE;
  paging_switch_to_kernel_directory();
  paging_destroy_user_directory(task->cr3);
  task->cr3 = 0;

  __asm__ volatile("sti");
  shell_run();

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static struct task *task_find_child(int parent_pid, int pid) {
  for (int i = 0; i < MAX_TASKS; i++) {
    struct task *task = &tasks[i];

    if (task->state == TASK_UNUSED || task->parent_pid != parent_pid) {
      continue;
    }

    if (pid > 0 && task->pid != pid) {
      continue;
    }

    return task;
  }

  return 0;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
int task_wait_pid(int pid, int *exit_code_out) {
  struct task *parent = current_task;

  if (!parent) {
    return -1;
  }

  for (;;) {
    struct task *child = task_find_child(parent->pid, pid);

    if (!child) {
      return -1;
    }

    if (child->state == TASK_ZOMBIE) {
      int code = child->exit_code;
      int child_pid = child->pid;

      if (exit_code_out) {
        *exit_code_out = code;
      }

      syscall_fs_close_for_pid(child_pid);

      log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                  "Task %d reaped child %d (status %d)", parent->pid, child_pid,
                  code);

      task_free_slot(child);
      return child_pid;
    }

    parent->wait_pid = child->pid;
    parent->wait_stdin = 0;
    parent->state = TASK_WAITING;
    task_schedule();
  }
}
