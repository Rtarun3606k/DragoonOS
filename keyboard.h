#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_initialize(void);
void keyboard_handle_interrupt(void);
int keyboard_read_line(char *buffer, unsigned int capacity);

#endif
