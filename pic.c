#include "pic.h"
#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_ICW1_INIT 0x10
#define PIC_ICW1_ICW4 0x01
#define PIC_ICW4_8086 0x01

void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
  outb(PIC1_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
  outb(PIC2_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);

  outb(PIC1_DATA, master_offset);
  outb(PIC2_DATA, slave_offset);

  outb(PIC1_DATA, 0x04);
  outb(PIC2_DATA, 0x02);

  outb(PIC1_DATA, PIC_ICW4_8086);
  outb(PIC2_DATA, PIC_ICW4_8086);
}

void pic_mask_all(void) {
  outb(0x21, 0xFF); // Mask all master PIC IRQs
  outb(0xA1, 0xFF); // Mask all slave PIC IRQs
}

void pic_send_eoi(uint8_t irq) {
  if (irq >= 8) {
    outb(0xA0, 0x20); // Acknowledge slave PIC
  }

  outb(0x20, 0x20); // Acknowledge master PIC
}

void pic_unmask_irq(uint8_t irq) {
  uint16_t port = (irq < 8) ? 0x21 : 0xA1;
  uint8_t line = irq % 8;

  uint8_t mask = inb(port);
  mask &= ~(1U << line);
  outb(port, mask);
}
