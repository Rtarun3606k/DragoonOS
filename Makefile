NASM        := nasm
CC          := gcc
LD          := ld
GENISOIMAGE := genisoimage
BOCHS       := bochs

BUILD_DIR   := build
ISO_DIR     := iso
KERNEL      := $(BUILD_DIR)/kernel.elf
ISO_IMAGE   := os.iso

CFLAGS := -m32 -std=gnu11 -ffreestanding \
          -nostdlib -fno-builtin \
          -fno-stack-protector -nostartfiles \
          -nodefaultlibs -Wall -Wextra -Werror

.PHONY: all kernel iso run clean

all: iso

# Create the build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assembly objects
$(BUILD_DIR)/loader.o: loader.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/io.o: io.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

# C objects
$(BUILD_DIR)/kmain.o: kmain.c framebuffer.h serial.h gdt.h idt.h| $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/framebuffer.o: framebuffer.c framebuffer.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: serial.c serial.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/log.o: log.c log.h framebuffer.h serial.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: gdt.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/gdt_c.o: gdt.c gdt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: idt.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/idt_c.o: idt.c idt.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/interrupt_stubs.o: interrupt_stubs.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/interrupts.o: interrupts.c interrupts.h log.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/pic.o: pic.c pic.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: timer.c timer.h io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: keyboard.c keyboard.h io.h pic.h log.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link the kernel
$(KERNEL): $(BUILD_DIR)/loader.o \
           $(BUILD_DIR)/kmain.o \
           $(BUILD_DIR)/io.o \
           $(BUILD_DIR)/framebuffer.o \
           $(BUILD_DIR)/serial.o \
					 $(BUILD_DIR)/log.o \
					 $(BUILD_DIR)/gdt_c.o \
					 $(BUILD_DIR)/gdt.o \
					 $(BUILD_DIR)/idt.o \
					 $(BUILD_DIR)/idt_c.o \
					 $(BUILD_DIR)/interrupt_stubs.o \
					 $(BUILD_DIR)/interrupts.o \
					 $(BUILD_DIR)/pic.o \
					 $(BUILD_DIR)/timer.o \
					 $(BUILD_DIR)/keyboard.o \
           link.ld
	$(LD) -T link.ld -m elf_i386 \
		$(BUILD_DIR)/loader.o \
		$(BUILD_DIR)/kmain.o \
		$(BUILD_DIR)/io.o \
		$(BUILD_DIR)/framebuffer.o \
		$(BUILD_DIR)/serial.o \
		$(BUILD_DIR)/log.o \
		$(BUILD_DIR)/gdt.o \
		$(BUILD_DIR)/gdt_c.o \
		$(BUILD_DIR)/idt.o \
		$(BUILD_DIR)/idt_c.o \
		$(BUILD_DIR)/interrupt_stubs.o \
		$(BUILD_DIR)/interrupts.o \
		$(BUILD_DIR)/pic.o \
		$(BUILD_DIR)/timer.o \
		$(BUILD_DIR)/keyboard.o \
		-o $@

# Copy the kernel into the ISO staging directory
kernel: $(KERNEL)
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf

# Generate a bootable ISO
iso: kernel
	$(GENISOIMAGE) -R \
		-b boot/grub/stage2_eltorito \
		-no-emul-boot \
		-boot-load-size 4 \
		-A os \
		-input-charset utf8 \
		-quiet \
		-boot-info-table \
		-o $(ISO_IMAGE) \
		$(ISO_DIR)

# Run in Bochs
run: iso
	$(BOCHS) -f bochs.txt -q

# Remove generated build artifacts
clean:
	rm -rf $(BUILD_DIR) $(ISO_IMAGE)

