#include "keyboard.h"
#include "framebuffer.h"
#include "io.h"
#include "log.h"
#include "pic.h"
#include "task.h"
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 128
#define STDIN_BUFFER_SIZE 512

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int caps_lock = 0;
static int extended_scancode = 0;

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile unsigned int keyboard_buffer_length = 0;

static char completed_line[KEYBOARD_BUFFER_SIZE];
static volatile int line_ready = 0;

/*
 * Canonical stdin queue shared by Ring 3 readers.  The keyboard IRQ is the
 * only producer; SYS_READ drains it from the task that owns the buffer.
 * head == tail means empty.  A one-byte gap distinguishes full from empty.
 */
static char stdin_queue[STDIN_BUFFER_SIZE];
static volatile unsigned int stdin_head = 0;
static volatile unsigned int stdin_tail = 0;
static volatile int stdin_eof = 0;

void keyboard_initialize(void) {
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Keyboard handler initialized");
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void stdin_push(char c) {
  unsigned int next = (stdin_tail + 1U) % STDIN_BUFFER_SIZE;

  if (next == stdin_head) {
    /* Queue full: drop the character rather than corrupting the ring. */
    return;
  }

  stdin_queue[stdin_tail] = c;
  stdin_tail = next;
}

int keyboard_stdin_read(char *buffer, unsigned int capacity) {
  if (!buffer || capacity == 0) {
    return 0;
  }

  unsigned int count = 0;

  while (count < capacity && stdin_head != stdin_tail) {
    buffer[count++] = stdin_queue[stdin_head];
    stdin_head = (stdin_head + 1U) % STDIN_BUFFER_SIZE;
  }

  return (int)count;
}

int keyboard_stdin_available(void) {
  return stdin_head != stdin_tail;
}

int keyboard_stdin_eof(void) { return stdin_eof; }

void keyboard_stdin_clear_eof(void) { stdin_eof = 0; }

static char scancode_to_char(uint8_t scancode, int shifted) {
  static const char keymap_normal[128] = {
      [0x02] = '1',  [0x03] = '2',  [0x04] = '3',  [0x05] = '4',
      [0x06] = '5',  [0x07] = '6',  [0x08] = '7',  [0x09] = '8',
      [0x0A] = '9',  [0x0B] = '0',  [0x0C] = '-',  [0x0D] = '=',
      [0x0F] = '\t', [0x10] = 'q',  [0x11] = 'w',  [0x12] = 'e',
      [0x13] = 'r',  [0x14] = 't',  [0x15] = 'y',  [0x16] = 'u',
      [0x17] = 'i',  [0x18] = 'o',  [0x19] = 'p',  [0x1A] = '[',
      [0x1B] = ']',  [0x1C] = '\n', [0x1E] = 'a',  [0x1F] = 's',
      [0x20] = 'd',  [0x21] = 'f',  [0x22] = 'g',  [0x23] = 'h',
      [0x24] = 'j',  [0x25] = 'k',  [0x26] = 'l',  [0x27] = ';',
      [0x28] = '\'', [0x29] = '`',  [0x2B] = '\\', [0x2C] = 'z',
      [0x2D] = 'x',  [0x2E] = 'c',  [0x2F] = 'v',  [0x30] = 'b',
      [0x31] = 'n',  [0x32] = 'm',  [0x33] = ',',  [0x34] = '.',
      [0x35] = '/',  [0x37] = '*',  [0x39] = ' ',  [0x47] = '7',
      [0x48] = '8',  [0x49] = '9',  [0x4A] = '-',  [0x4B] = '4',
      [0x4C] = '5',  [0x4D] = '6',  [0x4E] = '+',  [0x4F] = '1',
      [0x50] = '2',  [0x51] = '3',  [0x52] = '0',  [0x53] = '.',
  };

  static const char keymap_shifted[128] = {
      [0x02] = '!',  [0x03] = '@',  [0x04] = '#',  [0x05] = '$',
      [0x06] = '%',  [0x07] = '^',  [0x08] = '&',  [0x09] = '*',
      [0x0A] = '(',  [0x0B] = ')',  [0x0C] = '_',  [0x0D] = '+',
      [0x0F] = '\t', [0x10] = 'Q',  [0x11] = 'W',  [0x12] = 'E',
      [0x13] = 'R',  [0x14] = 'T',  [0x15] = 'Y',  [0x16] = 'U',
      [0x17] = 'I',  [0x18] = 'O',  [0x19] = 'P',  [0x1A] = '{',
      [0x1B] = '}',  [0x1C] = '\n', [0x1E] = 'A',  [0x1F] = 'S',
      [0x20] = 'D',  [0x21] = 'F',  [0x22] = 'G',  [0x23] = 'H',
      [0x24] = 'J',  [0x25] = 'K',  [0x26] = 'L',  [0x27] = ':',
      [0x28] = '"',  [0x29] = '~',  [0x2B] = '|',  [0x2C] = 'Z',
      [0x2D] = 'X',  [0x2E] = 'C',  [0x2F] = 'V',  [0x30] = 'B',
      [0x31] = 'N',  [0x32] = 'M',  [0x33] = '<',  [0x34] = '>',
      [0x35] = '?',  [0x37] = '*',  [0x39] = ' ',  [0x47] = '7',
      [0x48] = '8',  [0x49] = '9',  [0x4A] = '-',  [0x4B] = '4',
      [0x4C] = '5',  [0x4D] = '6',  [0x4E] = '+',  [0x4F] = '1',
      [0x50] = '2',  [0x51] = '3',  [0x52] = '0',  [0x53] = '.',
  };

  if (scancode >= 128) {
    return '\0';
  }

  char base = keymap_normal[scancode];
  if (base >= 'a' && base <= 'z') {
    int upper = shifted ^ caps_lock;
    return upper ? keymap_shifted[scancode] : base;
  }

  return shifted ? keymap_shifted[scancode] : base;
}

static void submit_line(void) {
  keyboard_buffer[keyboard_buffer_length] = '\0';

  if (!line_ready) {
    for (unsigned int i = 0; i <= keyboard_buffer_length; i++) {
      completed_line[i] = keyboard_buffer[i];
    }
    line_ready = 1;
  }

  for (unsigned int i = 0; i < keyboard_buffer_length; i++) {
    stdin_push(keyboard_buffer[i]);
  }
  stdin_push('\n');

  keyboard_buffer_length = 0;
  fb_write("\n", 1);
  task_wake_stdin();
}

void keyboard_handle_interrupt(void) {
  uint8_t scancode = inb(0x60);

  if (scancode == 0xE0) {
    extended_scancode = 1;
    pic_send_eoi(1);
    return;
  }

  if (extended_scancode) {
    extended_scancode = 0;
    if (scancode == 0x1D) {
      ctrl_pressed = 1;
    } else if (scancode == 0x9D) {
      ctrl_pressed = 0;
    } else if (scancode == 0x35) {
      /* Numpad '/' */
      if (keyboard_buffer_length < KEYBOARD_BUFFER_SIZE - 1) {
        keyboard_buffer[keyboard_buffer_length++] = '/';
        fb_write("/", 1);
      }
    } else if (scancode == 0x1C) {
      /* Numpad Enter */
      submit_line();
    }
    pic_send_eoi(1);
    return;
  }

  if (scancode == 0x2A || scancode == 0x36) {
    shift_pressed = 1;
  } else if (scancode == 0xAA || scancode == 0xB6) {
    shift_pressed = 0;
  } else if (scancode == 0x1D) {
    ctrl_pressed = 1;
  } else if (scancode == 0x9D) {
    ctrl_pressed = 0;
  } else if (scancode == 0x3A) {
    caps_lock = !caps_lock;
  } else if (!(scancode & 0x80)) {
    if (scancode == 0x0E) {
      if (keyboard_buffer_length > 0) {
        keyboard_buffer_length--;
        fb_backspace();
      }
    } else if (scancode == 0x1C) {
      submit_line();
    } else {
      char c = scancode_to_char(scancode, shift_pressed);

      /* Ctrl+C cancels the current line and returns an empty prompt */
      if (ctrl_pressed && (c == 'c' || c == 'C')) {
        fb_write("^C", 2);
        keyboard_buffer_length = 0;
        submit_line();
        pic_send_eoi(1);
        return;
      }

      /* Ctrl+D signals EOF to stdin readers.  A partially typed line is
       * delivered first, like a terminal in canonical mode. */
      if (ctrl_pressed && (c == 'd' || c == 'D')) {
        if (keyboard_buffer_length > 0) {
          submit_line();
        } else {
          stdin_eof = 1;
          task_wake_stdin();
        }
        pic_send_eoi(1);
        return;
      }

      if (c == '\t') {
        if (keyboard_buffer_length < KEYBOARD_BUFFER_SIZE - 1) {
          keyboard_buffer[keyboard_buffer_length++] = ' ';
          fb_write(" ", 1);
        }
      } else if (c != '\0' && c != '\n' &&
                 keyboard_buffer_length < KEYBOARD_BUFFER_SIZE - 1) {
        keyboard_buffer[keyboard_buffer_length++] = c;
        fb_write(&c, 1);
      }
    }
  }

  pic_send_eoi(1);
}

int keyboard_read_line(char *buffer, unsigned int capacity) {
  if (buffer == 0 || capacity == 0) {
    return -1;
  }

  while (!line_ready) {
    __asm__ volatile("hlt");
  }

  unsigned int i = 0;
  while (i < capacity - 1 && completed_line[i] != '\0') {
    buffer[i] = completed_line[i];
    i++;
  }

  buffer[i] = '\0';
  line_ready = 0;
  return (int)i;
}
