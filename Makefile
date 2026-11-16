NASM        := nasm
CC          := gcc
LD          := ld
GENISOIMAGE := genisoimage
BOCHS       := bochs

BUILD_DIR   := build
ISO_DIR     := iso
KERNEL      := $(BUILD_DIR)/kernel.elf
ISO_IMAGE   := os.iso
DISK_IMAGE  := disk.img

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
             $(wildcard src/drivers/*.c) \
             $(wildcard src/fs/*.c)

ASM_SOURCES := $(wildcard src/boot/*.s) \
               $(wildcard src/arch/*.s) \
               $(wildcard src/cpu/*.s)

USER_SOURCE := user/terminal.c
USER_LINKER := user/linker.ld
USER_OBJECT := $(BUILD_DIR)/user/terminal.o
MODULE_BINARY := $(ISO_DIR)/modules/program

USER_CFLAGS := -m32 -std=gnu11 -ffreestanding \
               -fno-pie -fno-pic -fno-stack-protector \
               -Wall -Wextra -Werror
USER_LDFLAGS := -m elf_i386 -no-pie -T $(USER_LINKER)

# TinyGo User-Space Binaries Configuration
TINYGOROOT    ?= $(abspath user/go/.tinygo-root)
GO_TOOLCHAIN  ?= go1.24.0
TINYGO_TARGET := $(abspath user/go/dragoon.json)
GO_RUNTIME    := $(wildcard user/go/*.c) $(wildcard user/go/*.S) $(wildcard user/go/*.go) \
                 $(wildcard user/go/patches/*.c) \
                 $(wildcard user/go/patches/src/os/*.go) \
                 $(wildcard user/go/patches/src/runtime/*.go)
GO_CMDS       := $(notdir $(wildcard user/go/cmd/*))
GO_ELFS       := $(addprefix $(BUILD_DIR)/go/,$(addsuffix .elf,$(GO_CMDS)))

C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.c.o,$(C_SOURCES))
ASM_OBJECTS := $(patsubst src/%.s,$(BUILD_DIR)/%.s.o,$(ASM_SOURCES))

LOADER_OBJECT := $(BUILD_DIR)/boot/loader.s.o
OTHER_OBJECTS := $(filter-out $(LOADER_OBJECT),$(ASM_OBJECTS)) \
                 $(C_OBJECTS)
OBJECTS := $(LOADER_OBJECT) $(OTHER_OBJECTS)

.PHONY: all kernel iso go-apps bundle-go run clean

all: iso $(DISK_IMAGE) bundle-go

# Compile and link Ring 3 user-space ELF module
$(USER_OBJECT): $(USER_SOURCE)
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(MODULE_BINARY): $(USER_OBJECT) $(USER_LINKER)
	mkdir -p $(dir $@)
	$(LD) $(USER_LDFLAGS) $(USER_OBJECT) -o $@

# Compile TinyGo user-space ELF binaries for every directory in user/go/cmd/*
$(BUILD_DIR)/go/%.elf: user/go/cmd/%/*.go $(GO_RUNTIME) user/go/dragoon.json $(USER_LINKER)
	mkdir -p $(dir $@)
	cd user/go && TINYGOROOT=$(TINYGOROOT) GOTOOLCHAIN=$(GO_TOOLCHAIN) \
		tinygo build -target=$(TINYGO_TARGET) -o $(abspath $@) ./cmd/$*

go-apps: $(GO_ELFS)

# Copy all compiled Go binaries into disk.img ::/bin/
bundle-go: $(DISK_IMAGE) $(GO_ELFS)
	@for cmd in $(GO_CMDS); do \
		mcopy -o -i $(DISK_IMAGE) $(BUILD_DIR)/go/$$cmd.elf ::/bin/$$cmd.bin; \
	done

# Compile FatFs files without -Werror (must come BEFORE $(BUILD_DIR)/%.c.o)
$(BUILD_DIR)/fs/%.c.o: src/fs/%.c
	mkdir -p $(dir $@)
	$(CC) $(filter-out -Werror,$(CFLAGS)) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Compile kernel C files
$(BUILD_DIR)/%.c.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# Assemble kernel NASM files
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
iso: kernel $(MODULE_BINARY)
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

# Create persistent 64 MiB FAT32 disk image only if it does not exist yet
$(DISK_IMAGE):
	dd if=/dev/zero of=$(DISK_IMAGE) bs=512 count=131040
	mkfs.vfat -F 32 $(DISK_IMAGE)
	mmd -i $(DISK_IMAGE) ::/bin ::/home
	echo 'Welcome to DragoonOS persistent storage!' | mcopy -i $(DISK_IMAGE) - ::/home/readme.txt

# Run in Bochs
run: all
	$(BOCHS) -f bochs.txt -q

# Clean generated files (keeps disk.img intact)
clean:
	rm -rf $(BUILD_DIR) $(ISO_IMAGE) $(MODULE_BINARY)

-include $(C_OBJECTS:.o=.d)
