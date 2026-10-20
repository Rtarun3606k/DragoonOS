#include "idt.h"
#include <stdint.h>
// total size: 8 bytes IDT entry requires
struct idt_entry {

  uint16_t offset_low;
  uint16_t selector;
  uint8_t reserved;
  uint8_t type_attributes;
  uint16_t offset_high;

} __attribute__((packed));

struct idt_pointer {
  uint16_t limit;
  uint32_t base;

} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idtr;

extern void idt_load(uint32_t address);

extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);

static void idt_set_entry(uint8_t vector, uint32_t handler_address,
                          uint16_t selector, uint8_t type_attributes) {

  idt[vector].offset_low = handler_address & 0xFFFF;
  idt[vector].selector = selector;
  idt[vector].reserved = 0;
  idt[vector].type_attributes = type_attributes;
  idt[vector].offset_high = (handler_address >> 16) & 0xFFFF;
}

static void idt_install_exception(int vector, void (*handler)(void)) {
  idt_set_entry(vector, (uint32_t)handler, 0x08, 0x8E);
}

void idt_initialize(void) {
  void (*handlers[32])(void) = {isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,
                                isr7,  isr8,  isr9,  isr10, isr11, isr12, isr13,
                                isr14, isr15, isr16, isr17, isr18, isr19, isr20,
                                isr21, isr22, isr23, isr24, isr25, isr26, isr27,
                                isr28, isr29, isr30, isr31};

  idtr.limit = sizeof(idt) - 1;
  idtr.base = (uint32_t)idt;

  for (int i = 0; i < 32; i++) {
    idt_install_exception(i, handlers[i]);
  }

  void (*irq_handlers[16])(void) = {
    isr32, isr33, isr34, isr35,
    isr36, isr37, isr38, isr39,
    isr40, isr41, isr42, isr43,
    isr44, isr45, isr46, isr47
};

for (int i = 0; i < 16; i++) {
    idt_install_exception(32 + i, irq_handlers[i]);
}

  idt_load((uint32_t)&idtr);
}
