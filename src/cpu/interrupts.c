#include "interrupts.h"
#include "keyboard.h"
#include "log.h"
#include "pic.h"
#include "syscall.h"
#include "task.h"

#define PAGE_FAULT_VECTOR 14U
#define USER_CS_RPL 0x3U

#define PAGE_FAULT_PROTECTION 0x1U
#define PAGE_FAULT_WRITE 0x2U
#define PAGE_FAULT_USER 0x4U
#define PAGE_FAULT_RESERVED 0x8U
#define PAGE_FAULT_INSTRUCTION 0x10U

static volatile uint32_t ticks = 0;
uint32_t timer_ticks(void) { return ticks; }

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static uint32_t read_cr2(void) {
  uint32_t address;

  __asm__ volatile("mov %%cr2, %0" : "=r"(address));

  return address;
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void log_page_fault(const struct interrupt_frame *frame,
                           uint32_t address) {
  uint32_t error = frame->error_code;

  log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
              "Page fault at 0x%x: %s, %s, %s%s%s, eip=0x%x, cs=0x%x, "
              "eflags=0x%x",
              address,
              (error & PAGE_FAULT_PROTECTION) ? "protection violation"
                                              : "page not present",
              (error & PAGE_FAULT_WRITE) ? "write" : "read",
              (error & PAGE_FAULT_USER) ? "user mode" : "kernel mode",
              (error & PAGE_FAULT_RESERVED) ? ", reserved bit set" : "",
              (error & PAGE_FAULT_INSTRUCTION) ? ", instruction fetch" : "",
              frame->eip, frame->cs, frame->eflags);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
static void page_fault_handler(struct interrupt_frame *frame) {
  uint32_t address = read_cr2();
  int user_mode = (frame->cs & USER_CS_RPL) == USER_CS_RPL;

  log_page_fault(frame, address);

  if (user_mode) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Terminating user process after page fault at eip=0x%x",
                frame->eip);
    syscall_terminate_user(-1);
  }

  log_message(LOG_FATAL, LOG_OUTPUT_SERIAL,
              "Page fault in kernel mode at 0x%x, halting", address);

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------
void interrupt_handler(struct interrupt_frame *frame) {
  uint32_t vector = frame->interrupt_number;

  if (vector == 0x80) {
    syscall_handler(frame);
    return;
  }

  if (vector == PAGE_FAULT_VECTOR) {
    page_fault_handler(frame);
    return;
  }

  if (vector >= 32 && vector <= 47) {
    uint8_t irq = vector - 32;

    // log_message(LOG_DEBUG, LOG_OUTPUT_SERIAL, "Hardware IRQ %u received",
    // irq);

    if (irq == 0) {
      ticks++;
      pic_send_eoi(0);
      task_timer_tick(frame->cs);
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
