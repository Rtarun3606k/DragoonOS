#include "framebuffer.h"
#include "gdt.h"
#include "idt.h"
#include "interrupts.h"
#include "io.h"
#include "keyboard.h"
#include "log.h"
#include "pic.h"
#include "serial.h"
#include "timer.h"

extern void isr3(void);

int kernel_strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  // Returns 0 if they are identical
  return *(unsigned char *)s1 - *(unsigned char *)s2;
}

void kmain(void) {

  // scroll test
  //  char message[100] = "DragoonOS scrolling test\n";
  //
  //  int current_length = 25; // Length of the initial string
  //
  //  for (int i = 0; i < 40; i++) {
  //    message[current_length] = '1';
  //    current_length++;
  //
  //    message[current_length] = '\n';
  //
  //    message[current_length + 1] = '\0';
  //
  //    write(message, current_length + 1);
  //  }

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

  // gloabl desc table
  gdt_initialize();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "GDT loaded successfully");
  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "GDT loaded successfully");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Before IDT initialization");
  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "Before IDT initialization");

  // instruction sdescription table
  idt_initialize();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "IDT loaded successfully");
  log_message(LOG_INFO, LOG_OUTPUT_FRAME_BUFFER, "IDT loaded successfully");

  // interrupts works
  //  isr3();
  //  __asm__ volatile("int3");

  pic_remap(0x20, 0x28);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIC remapped successfully");

  pic_mask_all();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "All PIC IRQs masked");

  uint8_t master_mask = inb(0x21);
  uint8_t slave_mask = inb(0xA1);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIC masks: master=0x%x slave=0x%x",
              master_mask, slave_mask);

  timer_initialize(100);
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIT initialized at 100 Hz");
  //
  //

  keyboard_initialize();
  pic_unmask_irq(1);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Keyboard IRQ1 unmasked");

  pic_unmask_irq(0);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Timer IRQ0 unmasked");

  __asm__ volatile("sti");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "CPU interrupts enabled");

  // test input and read line from bios
  //
  char input[128];

  for (;;) {
    fb_write("input> ", 7);

    int length = keyboard_read_line(input, sizeof(input));

    if (length >= 0) {
      fb_write("received: ", 10);
      fb_write(input, (unsigned int)length);
      fb_write("\n", 1);
      if (kernel_strcmp(input, "clear") == 0) {
        fb_clear();
      }
    }
  }

  // Infinite halt loop to stop the CPU core safely
  // for (;;) {
  //   __asm__ volatile("hlt");
  //   // log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Timer ticks: %u",
  //   // timer_ticks());
  // }
}
