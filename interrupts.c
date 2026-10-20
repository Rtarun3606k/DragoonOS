#include "interrupts.h"
#include "keyboard.h"
#include "log.h"
#include "pic.h"

static volatile uint32_t ticks = 0;
uint32_t timer_ticks(void) { return ticks; }

void interrupt_handler(struct interrupt_frame *frame) {
  uint32_t vector = frame->interrupt_number;

  if (vector >= 32 && vector <= 47) {
    uint8_t irq = vector - 32;

    log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Hardware IRQ %u received", irq);

    if (irq == 0) {
      ticks++;
      pic_send_eoi(0);
      return;
    }

    if (irq == 1) {
      keyboard_handle_interrupt();
      return;
    }

    pic_send_eoi(irq);
    return;
  }

  log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "CPU exception %u, error code: %u",
              vector, frame->error_code);

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}
