#include "ff.h"
#include "framebuffer.h"
#include "gdt.h"
#include "heap.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "log.h"
#include "memory.h"
#include "module.h"
#include "multiboot.h"
#include "paging.h"
#include "pic.h"
#include "serial.h"
#include "shell.h"
#include "timer.h"

#define HIGHER_HALF_BASE 0xC0000000
#define TEST_VIRTUAL_ADDRESS 0xD0000000U

static FATFS root_fs;

static void log_memory_map(multiboot_info_t *mbi) {

  // type 1 avaliabel ram
  // 2 is reserved
  // 3 is ACPI reclamiable ram if wanted

  if (!(mbi->flags & MULTIBOOT_INFO_MMAP)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "GRUB did not provide a memory map");
    return;
  }

  unsigned int offset = 0;
  unsigned int mmap_virtual = mbi->mmap_addr + HIGHER_HALF_BASE;

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Memory map length: %u bytes",
              mbi->mmap_length);

  while (offset < mbi->mmap_length) {
    multiboot_mmap_entry_t *entry =
        (multiboot_mmap_entry_t *)(mmap_virtual + offset);

    /* Each entry must contain at least 20 bytes after size. */
    if (entry->size < 20 ||
        entry->size + sizeof(entry->size) > mbi->mmap_length - offset) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Invalid memory-map entry");
      return;
    }

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Memory region: base_hi=0x%x base_lo=0x%x "
                "length_hi=0x%x length_lo=0x%x type=%u",
                entry->addr_high, entry->addr_low, entry->len_high,
                entry->len_low, entry->type);

    offset += entry->size + sizeof(entry->size);
  }
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

static void paging_test(void) {
  unsigned int frame = frame_alloc();

  if (frame == 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Paging test: frame allocation failed");
    return;
  }

  if (map_page(TEST_VIRTUAL_ADDRESS, frame, PAGE_WRITABLE) != 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Paging test: mapping failed");
    frame_free(frame);
    return;
  }

  volatile unsigned int *memory = (volatile unsigned int *)TEST_VIRTUAL_ADDRESS;

  memory[0] = 0xDEADBEEF;
  memory[1023] = 0xCAFEBABE;

  if (memory[0] == 0xDEADBEEF && memory[1023] == 0xCAFEBABE) {
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Paging test: virtual memory read/write successful");
  } else {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Paging test: read/write verification failed");
  }

  unsigned int unmapped = unmap_page(TEST_VIRTUAL_ADDRESS);

  if (unmapped == frame) {
    frame_free(unmapped);
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Paging test: page unmapped and frame released");
  } else {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Paging test: unmapping failed");
  }
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

void usermode_test(void) {
  unsigned int user_code_virt = 0x00400000U;
  unsigned int user_stack_virt = 0x00800000U;

  unsigned int code_phys = frame_alloc();
  unsigned int stack_phys = frame_alloc();

  if (!code_phys || !stack_phys) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Usermode test: frame_alloc failed");
    return;
  }

  if (map_page(user_code_virt, code_phys, PAGE_WRITABLE | PAGE_USER) != 0 ||
      map_page(user_stack_virt, stack_phys, PAGE_WRITABLE | PAGE_USER) != 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Usermode test: map_page failed");
    return;
  }

  const char *msg = "Hello from Ring 3 via int 0x80!\n";
  unsigned int msg_virt = user_code_virt + 0x100U;
  char *msg_dest = (char *)msg_virt;
  for (unsigned int i = 0; i < 32; i++) {
    msg_dest[i] = msg[i];
  }
  /*
   * 11-byte x86 user-mode payload:
   *   B9 00 00 00 03   mov ecx, 0x03000000   ; Spin ~50M cycles so PIT IRQ0
   * fires in Ring 3 49               dec ecx 75 FD            jnz -3 FA cli ;
   * Privileged instruction -> triggers #GP in Ring 3! EB FE            jmp $
   */
  unsigned char user_code[] = {
      0xB8, 0x04, 0x00, 0x00, 0x00, 0xBB, 0x01, 0x00, 0x00, 0x00, 0xB9, 0x00,
      0x01, 0x40, 0x00, 0xBA, 0x20, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xB8, 0x01,
      0x00, 0x00, 0x00, 0xBB, 0x00, 0x00, 0x00, 0x00, 0xCD, 0x80, 0xEB, 0xFE};

  unsigned char *dest = (unsigned char *)user_code_virt;
  for (unsigned int i = 0; i < sizeof(user_code); i++) {
    dest[i] = user_code[i];
  }

  /* Allocate a 4 KiB kernel stack for Ring 3 -> Ring 0 interrupt transitions */
  void *kstack = kmalloc(4096);
  if (!kstack) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Usermode test: kstack alloc failed");
    return;
  }
  tss_set_stack((unsigned int)kstack + 4096U);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Entering Ring 3 at 0x%x...",
              user_code_virt);
  enter_usermode(user_code_virt, user_stack_virt + 4096U);
}
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

void kmain(unsigned int magic, unsigned int multiboot_info_address) {
  serial_initialize();

  if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Invalid Multiboot magic: 0x%x",
                magic);

    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  if (multiboot_info_address == 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Missing Multiboot information pointer");

    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  multiboot_info_t *mbi =
      (multiboot_info_t *)(multiboot_info_address + HIGHER_HALF_BASE);

  fb_write("Multiboot information received\n", 31);
  fb_write("DragoonOS framebuffer works!\n", 29);
  serial_write("DragoonOS serial output works!\r\n", 32);

  log_memory_map(mbi);
  memory_init(multiboot_info_address);
  paging_init();
  paging_test();
  heap_test();

  FRESULT mount_res = f_mount(&root_fs, "", 1);
  if (mount_res == FR_OK) {
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "FAT32 root filesystem mounted");
  } else {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "FAT32 mount failed: %d",
                mount_res);
  }
  // unsigned int before =
  // free_frames; // Don't access static free_frames directly.
  // unsigned int before = memory_get_free_frames();

  unsigned int frame = frame_alloc();

  if (frame != 0) {
    unsigned int virtual_address = 0xD0000000;

    int result = map_page(virtual_address, frame, PAGE_WRITABLE);

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Page mapping result: %d", result);

    unsigned int unmapped = unmap_page(virtual_address);

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Unmapped physical frame: 0x%x",
                unmapped);

    frame_free(unmapped);
  }
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Before GDT initialization");
  gdt_initialize();
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "GDT loaded successfully");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Before IDT initialization");
  idt_initialize();
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "IDT loaded successfully");

  fb_disable_cursor();
  pic_remap(0x20, 0x28);
  pic_mask_all();

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIC remapped and IRQs masked");

  uint8_t master_mask = inb(0x21);
  uint8_t slave_mask = inb(0xA1);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIC masks: master=0x%x slave=0x%x",
              master_mask, slave_mask);

  timer_initialize(100);
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "PIT initialized at 100 Hz");

  keyboard_initialize();
  pic_unmask_irq(1);
  pic_unmask_irq(0);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Keyboard IRQ1 and timer IRQ0 unmasked");

  __asm__ volatile("sti");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "CPU interrupts enabled");

  if (!module_load_and_run(mbi)) {
    fb_write("Module loading failed\n", 22);

    for (;;) {
      __asm__ volatile("cli; hlt");
    }
  }

  shell_run();
}
