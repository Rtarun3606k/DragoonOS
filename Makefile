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
          -nodefaultlibs -Wall -Wextra -Werror \
          -Iinclude

ASFLAGS := -f elf32
LDFLAGS := -T link.ld -m elf_i386

C_SOURCES := $(wildcard src/kernel/*.c) \
             $(wildcard src/modules/*.c) \
             $(wildcard src/cpu/*.c) \
             $(wildcard src/drivers/*.c)


ASM_SOURCES := $(wildcard src/boot/*.s) \
               $(wildcard src/arch/*.s) \
               $(wildcard src/cpu/*.s)


MODULE_SOURCE := src/modules/program.s
MODULE_BINARY := $(ISO_DIR)/modules/program

# Use distinct names for C and assembly objects.
# This prevents gdt.c and gdt.s from overwriting each other.
C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.c.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.s,$(BUILD_DIR)/%.s.o,$(ASM_SOURCES))

LOADER_OBJECT := $(BUILD_DIR)/boot/loader.s.o
OTHER_OBJECTS := $(filter-out $(LOADER_OBJECT),$(ASM_OBJECTS)) \
                 $(C_OBJECTS)
OBJECTS := $(LOADER_OBJECT) $(OTHER_OBJECTS)

.PHONY: all kernel iso run clean

all: iso


$(MODULE_BINARY): $(MODULE_SOURCE)
	mkdir -p $(dir $@)
	$(NASM) -f bin $< -o $@

# Compile C files
$(BUILD_DIR)/%.c.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Assemble NASM files
$(BUILD_DIR)/%.s.o: src/%.s
	mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) $< -o $@

# Link the kernel
$(KERNEL): $(OBJECTS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Stage kernel for GRUB
kernel: $(KERNEL)
	mkdir -p $(ISO_DIR)/boot
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf

# Generate ISO
iso: kernel  $(MODULE_BINARY)
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

# Clean generated files
clean:
	rm -rf $(BUILD_DIR) $(ISO_IMAGE)

-include $(C_OBJECTS:.o=.d)
