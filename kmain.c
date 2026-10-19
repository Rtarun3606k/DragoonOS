#include "framebuffer.h"
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

  write(screen_message, sizeof(screen_message) - 1);
  serial_write(serial_message, sizeof(serial_message) - 1);
  //
  // Infinite halt loop to stop the CPU core safely
  for (;;) {
    __asm__ volatile("hlt");
  }
}
