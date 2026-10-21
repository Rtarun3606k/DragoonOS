#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame {
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t saved_esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;

  uint32_t interrupt_number;
  uint32_t error_code;

  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
};

uint32_t timer_ticks(void);

void interrupt_handler(struct interrupt_frame *frame);

#endif
