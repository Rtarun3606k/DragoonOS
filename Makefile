
NASM         := nasm
CC           := gcc
LD           := ld
GENISOIMAGE  := genisoimage
BOCHS        := bochs

BUILD_DIR    := build
ISO_DIR      := iso
KERNEL       := $(BUILD_DIR)/kernel.elf
ISO_IMAGE    := os.iso

CFLAGS := -m32 -std=gnu11 -ffreestanding \
          -nostdlib -nostdinc -fno-builtin \
          -fno-stack-protector -nostartfiles \
          -nodefaultlibs -Wall -Wextra -Werror

.PHONY: all kernel iso run clean

all: iso

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/loader.o: loader.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/io.o: io.s | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/kmain.o: kmain.c io.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(BUILD_DIR)/loader.o $(BUILD_DIR)/kmain.o $(BUILD_DIR)/io.o link.ld
	$(LD) -T link.ld -m elf_i386 \
		$(BUILD_DIR)/loader.o \
		$(BUILD_DIR)/kmain.o \
		$(BUILD_DIR)/io.o \
		-o $@

kernel: $(KERNEL)
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf

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

run: iso
	$(BOCHS) -f bochs.txt -q

clean:
	rm -rf $(BUILD_DIR) $(ISO_IMAGE)

