#ifndef KEYBOARD_H
#define KEYBOARD_H

/* Line-based reader used by the kernel shell (no scheduler involvement). */
void keyboard_initialize(void);
void keyboard_handle_interrupt(void);
int keyboard_read_line(char *buffer, unsigned int capacity);

/*
 * Canonical stdin queue consumed by SYS_READ(fd == 0).  keyboard_stdin_read
 * never blocks; a Ring 3 reader is parked by the scheduler when the queue is
 * empty and woken from IRQ1 via task_wake_stdin().
 */
int keyboard_stdin_read(char *buffer, unsigned int capacity);
int keyboard_stdin_available(void);
int keyboard_stdin_eof(void);
void keyboard_stdin_clear_eof(void);

#endif
