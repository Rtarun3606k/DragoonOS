#include "framebuffer.h"
#include "gdt.h"
#include "log.h"
#include "serial.h"

void kmain(void) {
  // char message[100] = "DragoonOS scrolling test\n";
  //
  // int current_length = 25; // Length of the initial string
  //
  // for (int i = 0; i < 40; i++) {
  //   message[current_length] = '1';
  //   current_length++;
  //
  //   message[current_length] = '\n';
  //
  //   message[current_length + 1] = '\0';
  //
  //   write(message, current_length + 1);
  // }

  char screen_message[] = "DragoonOS framebuffer works!\n";
  char serial_message[] = "DragoonOS serial output works!\r\n";

  serial_initialize();

  fb_write(screen_message, sizeof(screen_message) - 1);
  serial_write(serial_message, sizeof(serial_message) - 1);

  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "DragoonOS initialized");

  log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Cursor position: %u", 42u);

  log_message(LOG_WARN, LOG_OUTPUT_FRAME_BUFFER, "Temperature: %d C", -5);

  log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Device %s failed with code 0x%x",
              "COM1", 0x3F8u);

  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "Progress: 100%%");

  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "Kernel initialized");

  log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Value: %d, hex: 0x%x, percent: %%",
              -42, 0x3F8u);

  log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Device %s failed", "COM1");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Before GDT initialization");
  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "Before GDT initialization");

  gdt_initialize();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "GDT loaded successfully");
  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "GDT loaded successfully");

  //
  // Infinite halt loop to stop the CPU core safely
  for (;;) {
    __asm__ volatile("hlt");
  }
}
