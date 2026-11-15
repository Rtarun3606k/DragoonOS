#include "memory.h"
#include "kernel_layout.h"
#include "log.h"
#include "multiboot.h"

#define HIGHER_HALF_BASE 0xC0000000U
#define MAX_FRAMES 1048576U
#define BITMAP_SIZE (MAX_FRAMES / 8U)
#define MAX_PHYSICAL_MEMORY 0x100000000ULL

static unsigned char frame_bitmap[BITMAP_SIZE]
    __attribute__((aligned(PAGE_SIZE)));

static unsigned int total_frames;
static unsigned int free_frames;

static void set_frame_used(unsigned int frame) {
  unsigned int byte = frame / 8U;
  unsigned int bit = frame % 8U;

  if (!(frame_bitmap[byte] & (1U << bit))) {
    frame_bitmap[byte] |= (unsigned char)(1U << bit);
    free_frames--;
  }
}

static void set_frame_free(unsigned int frame) {
  unsigned int byte = frame / 8U;
  unsigned int bit = frame % 8U;

  if (frame_bitmap[byte] & (1U << bit)) {
    frame_bitmap[byte] &= (unsigned char)~(1U << bit);
    free_frames++;
  }
}

static void reserve_range(unsigned int start, unsigned int end) {
  // if (end <= start || start >= total_frames * PAGE_SIZE) {
  //   return;
  // }

  if (end <= start) {
    return;
  }

  unsigned int first = start / PAGE_SIZE;
  unsigned int last =
      (unsigned int)(((unsigned long long)end + PAGE_SIZE - 1U) / PAGE_SIZE);

  if (first >= total_frames) {
    return;
  }

  if (last > total_frames) {
    last = total_frames;
  }

  for (unsigned int frame = first; frame < last; frame++) {
    set_frame_used(frame);
  }
}

static void release_range(unsigned long long start, unsigned long long end) {
  if (end <= start || start >= MAX_PHYSICAL_MEMORY) {
    return;
  }

  if (end > MAX_PHYSICAL_MEMORY) {
    end = MAX_PHYSICAL_MEMORY;
  }

  unsigned long long first = (start + PAGE_SIZE - 1U) / PAGE_SIZE;
  unsigned long long last = end / PAGE_SIZE;

  if (last > total_frames) {
    last = total_frames;
  }

  for (unsigned long long frame = first; frame < last; frame++) {
    set_frame_free((unsigned int)frame);
  }
}

static multiboot_mmap_entry_t *get_mmap_entry(multiboot_info_t *mbi,
                                              unsigned int offset) {
  unsigned int address = mbi->mmap_addr + HIGHER_HALF_BASE + offset;

  return (multiboot_mmap_entry_t *)address;
}

void memory_init(unsigned int multiboot_info_address) {
  for (unsigned int i = 0; i < BITMAP_SIZE; i++) {
    frame_bitmap[i] = 0xFF;
  }

  total_frames = 0;
  free_frames = 0;

  multiboot_info_t *mbi =
      (multiboot_info_t *)(multiboot_info_address + HIGHER_HALF_BASE);

  if (!(mbi->flags & MULTIBOOT_INFO_MMAP)) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Frame allocator: no memory map");
    return;
  }

  /*
   * PASS 1:
   * Find the highest physical address in the memory map.
   */
  unsigned int offset = 0;
  unsigned long long highest_address = 0;

  while (offset < mbi->mmap_length) {
    multiboot_mmap_entry_t *entry = get_mmap_entry(mbi, offset);

    if (entry->size < 20 ||
        entry->size + sizeof(entry->size) > mbi->mmap_length - offset) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                  "Frame allocator: invalid memory map");
      return;
    }

    unsigned long long start =
        ((unsigned long long)entry->addr_high << 32) | entry->addr_low;

    unsigned long long length =
        ((unsigned long long)entry->len_high << 32) | entry->len_low;

    unsigned long long end = start + length;

    if (end < start) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                  "Frame allocator: memory range overflow");
      return;
    }

    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && end > highest_address) {
      highest_address = end;
    }

    offset += entry->size + sizeof(entry->size);
  }

  if (highest_address == 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Frame allocator: empty memory map");
    return;
  }

  if (highest_address > MAX_PHYSICAL_MEMORY) {
    highest_address = MAX_PHYSICAL_MEMORY;
  }

  total_frames = (unsigned int)((highest_address + PAGE_SIZE - 1U) / PAGE_SIZE);

  /*
   * PASS 2:
   * Release complete frames in available memory regions.
   */
  offset = 0;

  while (offset < mbi->mmap_length) {
    multiboot_mmap_entry_t *entry = get_mmap_entry(mbi, offset);

    unsigned long long start =
        ((unsigned long long)entry->addr_high << 32) | entry->addr_low;

    unsigned long long length =
        ((unsigned long long)entry->len_high << 32) | entry->len_low;

    unsigned long long end = start + length;

    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      release_range(start, end);
    }

    offset += entry->size + sizeof(entry->size);
  }

  /*
   * Reserve low memory and the entire kernel image,
   * including bootstrap structures and the bitmap.
   */
  reserve_range(0, 0x00100000U);

  reserve_range(0x00100000U, (unsigned int)__kernel_phys_end);

  /* Reserve the Multiboot information structure. */
  reserve_range(multiboot_info_address,
                multiboot_info_address + sizeof(multiboot_info_t));

  /* Reserve the memory map itself. */
  reserve_range(mbi->mmap_addr, mbi->mmap_addr + mbi->mmap_length);

  /* Reserve modules and their descriptors. */
  if (mbi->flags & MULTIBOOT_INFO_MODULES) {
    multiboot_module_t *modules =
        (multiboot_module_t *)(mbi->mods_addr + HIGHER_HALF_BASE);

    for (unsigned int i = 0; i < mbi->mods_count; i++) {
      reserve_range(modules[i].mod_start, modules[i].mod_end);
    }

    reserve_range(mbi->mods_addr,
                  mbi->mods_addr +
                      mbi->mods_count * sizeof(multiboot_module_t));
  }

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Physical frame allocator initialized");

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Total frames: %u", total_frames);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Free frames: %u", free_frames);
}

unsigned int frame_alloc(void) {
  if (free_frames == 0) {
    return 0;
  }

  for (unsigned int frame = 0; frame < total_frames; frame++) {
    unsigned int byte = frame / 8U;
    unsigned int bit = frame % 8U;

    if (!(frame_bitmap[byte] & (1U << bit))) {
      set_frame_used(frame);
      return frame * PAGE_SIZE;
    }
  }

  return 0;
}

void frame_free(unsigned int physical_address) {
  if (physical_address == 0 || physical_address % PAGE_SIZE != 0) {
    return;
  }

  unsigned int frame = physical_address / PAGE_SIZE;

  if (frame >= total_frames) {
    return;
  }

  set_frame_free(frame);
}

unsigned int memory_get_free_frames(void) { return free_frames; }
