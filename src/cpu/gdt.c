#include "gdt.h"

#include <stdint.h>

struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_middle;
  uint8_t access;
  uint8_t granularity;
  uint8_t base_high;
} __attribute__((packed));

struct gdt_pointer {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_pointer gdtr;

extern void gdt_load(uint32_t gdtr_address);

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
  gdt[index].base_low = base & 0xFFFF;
  gdt[index].base_middle = (base >> 16) & 0xFF;
  gdt[index].base_high = (base >> 24) & 0xFF;

  gdt[index].limit_low = limit & 0xFFFF;
  gdt[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);

  gdt[index].access = access;
}

void gdt_initialize(void) {
  gdtr.limit = sizeof(gdt) - 1;
  gdtr.base = (uint32_t)gdt;

  /* Null descriptor */
  gdt_set_entry(0, 0, 0, 0, 0);

  /* Kernel code: base 0, 4 GiB limit, ring 0 */
  gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);

  /* Kernel data: base 0, 4 GiB limit, ring 0 */
  gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

  gdt_load((uint32_t)&gdtr);
}
