#include "shell.h"
#include "framebuffer.h"
#include "keyboard.h"
#include "shutdown.h"
#include "string_utils.h"

void shell_run(void) {
  char input[128];

  for (;;) {
    fb_write("input> ", 7);

    int length = keyboard_read_line(input, sizeof(input));

    if (length < 0) {
      continue;
    }

    fb_write("received: ", 10);
    fb_write(input, (unsigned int)length);
    fb_write("\n", 1);

    if (kernel_strcmp(input, "clear") == 0) {
      fb_clear();
    } else if (kernel_strcmp(input, "halt") == 0 ||
               kernel_strcmp(input, "exit") == 0 ||
               kernel_strcmp(input, "quit") == 0) {
      fb_write("DragoonOS halted.\n", 18);
      kernel_halt();
    } else if (kernel_strcmp(input, "shutdown") == 0) {
      fb_write("Shutting down DragoonOS...\n", 27);
      kernel_shutdown();
    }
  }
}
