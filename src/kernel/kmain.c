#include "framebuffer.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "log.h"
#include "module.h"
#include "multiboot.h"
#include "pic.h"
#include "serial.h"
#include "shell.h"
#include "timer.h"

void kmain(unsigned int magic,
           unsigned int multiboot_info_address) {
    serial_initialize();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                    "Invalid Multiboot magic: 0x%x", magic);

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
        (multiboot_info_t *)multiboot_info_address;

    fb_write("Multiboot information received\n", 31);
    fb_write("DragoonOS framebuffer works!\n", 29);
    serial_write("DragoonOS serial output works!\r\n", 32);

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Before GDT initialization");
    gdt_initialize();
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "GDT loaded successfully");

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Before IDT initialization");
    idt_initialize();
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "IDT loaded successfully");

    fb_disable_cursor();
    pic_remap(0x20, 0x28);
    pic_mask_all();

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "PIC remapped and IRQs masked");

    uint8_t master_mask = inb(0x21);
    uint8_t slave_mask = inb(0xA1);

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "PIC masks: master=0x%x slave=0x%x",
                master_mask, slave_mask);

    timer_initialize(100);
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "PIT initialized at 100 Hz");

    keyboard_initialize();
    pic_unmask_irq(1);
    pic_unmask_irq(0);

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Keyboard IRQ1 and timer IRQ0 unmasked");

    __asm__ volatile("sti");

    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "CPU interrupts enabled");

    if (!module_load_and_run(mbi)) {
        fb_write("Module loading failed\n", 22);

        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    shell_run();
}
