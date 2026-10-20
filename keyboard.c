#include "keyboard.h"
#include "framebuffer.h"
#include "io.h"
#include "log.h"
#include "pic.h"

#define KEYBOARD_BUFFER_SIZE 128

static int shift_pressed = 0;
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile unsigned int keyboard_buffer_length = 0;

static char completed_line[KEYBOARD_BUFFER_SIZE];
static volatile int line_ready = 0;

void keyboard_initialize(void) {
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Keyboard handler initialized");
}

static char scancode_to_char(uint8_t scancode) {
  static const char keymap[128] = {
      [0x02] = '1', [0x03] = '2', [0x04] = '3',  [0x05] = '4', [0x06] = '5',
      [0x07] = '6', [0x08] = '7', [0x09] = '8',  [0x0A] = '9', [0x0B] = '0',
      [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',  [0x13] = 'r', [0x14] = 't',
      [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',  [0x18] = 'o', [0x19] = 'p',
      [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',  [0x21] = 'f', [0x22] = 'g',
      [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',  [0x26] = 'l', [0x2C] = 'z',
      [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',  [0x30] = 'b', [0x31] = 'n',
      [0x32] = 'm', [0x39] = ' ', [0x1C] = '\n',
  };

  if (scancode >= 128) {
    return '\0';
  }

  return keymap[scancode];
}

void keyboard_handle_interrupt(void) {
  uint8_t scancode = inb(0x60);

  log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Keyboard scancode: 0x%x",
              scancode);

  if (scancode == 0x2A || scancode == 0x36) {
    shift_pressed = 1;
  } else if (scancode == 0xAA || scancode == 0xB6) {
    shift_pressed = 0;
  } else if (!(scancode & 0x80)) {
    if (scancode == 0x0E) {
      if (keyboard_buffer_length > 0) {
        keyboard_buffer_length--;
        fb_backspace();
      }
    } else if (scancode == 0x1C) {
      /* Enter: save the completed line. */
      keyboard_buffer[keyboard_buffer_length] = '\0';

      if (!line_ready) {
        unsigned int i;

        for (i = 0; i <= keyboard_buffer_length; i++) {
          completed_line[i] = keyboard_buffer[i];
        }

        line_ready = 1;
      }

      keyboard_buffer_length = 0;
      fb_write("\n", 1);
    } else {
      char c = scancode_to_char(scancode);

      if (shift_pressed && c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
      }

      if (c != '\0' && c != '\n' &&
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
