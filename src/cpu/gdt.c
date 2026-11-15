#include "gdt.h"
#include "log.h"

#define GDT_ENTRIES 6

/* 8-byte GDT descriptor entry */
struct gdt_entry {
  unsigned short limit_low;
  unsigned short base_low;
  unsigned char base_middle;
  unsigned char access;
  unsigned char granularity;
  unsigned char base_high;
} __attribute__((packed));

/* Pointer structure passed to lgdt */
struct gdt_ptr {
  unsigned short limit;
  unsigned int base;
} __attribute__((packed));

/* 104-byte 32-bit x86 hardware Task State Segment */
struct tss_entry {
  unsigned int prev_tss;
  unsigned int
      esp0;         /* Kernel stack pointer loaded on Ring 3 -> Ring 0 switch */
  unsigned int ss0; /* Kernel stack segment loaded on Ring 3 -> Ring 0 switch */
  unsigned int esp1;
  unsigned int ss1;
  unsigned int esp2;
  unsigned int ss2;
  unsigned int cr3;
  unsigned int eip;
  unsigned int eflags;
  unsigned int eax;
  unsigned int ecx;
  unsigned int edx;
  unsigned int ebx;
  unsigned int esp;
  unsigned int ebp;
  unsigned int esi;
  unsigned int edi;
  unsigned int es;
  unsigned int cs;
  unsigned int ss;
  unsigned int ds;
  unsigned int fs;
  unsigned int gs;
  unsigned int ldt;
  unsigned short trap;
  unsigned short iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;
static struct tss_entry tss;

/* Defined in src/cpu/gdt.s */
extern void gdt_load(unsigned int gdt_ptr_addr);
extern void tss_flush(void);

static void memzero(void *ptr, unsigned int size) {
  unsigned char *bytes = (unsigned char *)ptr;
  for (unsigned int i = 0; i < size; i++) {
    bytes[i] = 0;
  }
}

static void gdt_set_gate(int num, unsigned int base, unsigned int limit,
                         unsigned char access, unsigned char gran) {
  gdt[num].base_low = (base & 0xFFFFU);
  gdt[num].base_middle = (base >> 16) & 0xFFU;
  gdt[num].base_high = (base >> 24) & 0xFFU;

  gdt[num].limit_low = (limit & 0xFFFFU);
  gdt[num].granularity = ((limit >> 16) & 0x0FU) | (gran & 0xF0U);
  gdt[num].access = access;
}

static void tss_init(int num, unsigned short ss0, unsigned int esp0) {
  unsigned int base = (unsigned int)&tss;
  unsigned int limit = sizeof(struct tss_entry) - 1U;

  /* Access 0x89: Present=1, DPL=0, System=0, Type=1001 (32-bit Available TSS)
   */
  gdt_set_gate(num, base, limit, 0x89, 0x00);

  memzero(&tss, sizeof(struct tss_entry));

  tss.ss0 = ss0;
  tss.esp0 = esp0;
  tss.iomap_base = sizeof(struct tss_entry);
}

void tss_set_stack(unsigned int kernel_esp) { tss.esp0 = kernel_esp; }

void gdt_initialize(void) {
  gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1U;
  gp.base = (unsigned int)&gdt;

  /* 0x00: Null descriptor */
  gdt_set_gate(0, 0, 0, 0, 0);

  /* 0x08: Kernel Code (Ring 0) - Base 0, Limit 4GB, Exec/Read */
  gdt_set_gate(1, 0, 0xFFFFFFFFU, 0x9A, 0xCF);

  /* 0x10: Kernel Data (Ring 0) - Base 0, Limit 4GB, Read/Write */
  gdt_set_gate(2, 0, 0xFFFFFFFFU, 0x92, 0xCF);

  /* 0x18: User Code (Ring 3) - Base 0, Limit 4GB, Exec/Read, DPL=3 */
  gdt_set_gate(3, 0, 0xFFFFFFFFU, 0xFA, 0xCF);

  /* 0x20: User Data (Ring 3) - Base 0, Limit 4GB, Read/Write, DPL=3 */
  gdt_set_gate(4, 0, 0xFFFFFFFFU, 0xF2, 0xCF);

  /* 0x28: Task State Segment (Ring 0) */
  tss_init(5, 0x10, 0x0);

  gdt_load((unsigned int)&gp);
  tss_flush();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "GDT and TSS loaded successfully");
}
