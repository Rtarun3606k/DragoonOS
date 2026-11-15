#include "heap.h"
#include "log.h"
#include "memory.h"
#include "paging.h"

#define HEAP_START 0xD0000000U
#define PAGE_SIZE 4096U
#define BLOCK_ALIGN 16U

/* Helper to align sizes upwards */
#define ALIGN_UP(size, align) (((size) + (align) - 1U) & ~((align) - 1U))

typedef struct heap_block {
  unsigned int size;    /* Size of the payload (excluding this header) */
  unsigned int is_free; /* 1 if free, 0 if in use */
  struct heap_block *next;
  struct heap_block *prev;
} heap_block_t;

static heap_block_t *heap_head = 0;
static heap_block_t *heap_tail = 0;
static unsigned int heap_current_end = HEAP_START;

/* Internal memory copy to avoid dependency on string_utils.c state */
static void _memcpy(unsigned char *dest, const unsigned char *src,
                    unsigned int n) {
  for (unsigned int i = 0; i < n; i++) {
    dest[i] = src[i];
  }
}

/* Expands the virtual heap by mapping new physical frames */
static heap_block_t *expand_heap(unsigned int size_needed) {
  unsigned int pages = ALIGN_UP(size_needed, PAGE_SIZE) / PAGE_SIZE;
  unsigned int start_addr = heap_current_end;

  for (unsigned int i = 0; i < pages; i++) {
    unsigned int phys = frame_alloc();
    if (!phys) {
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Heap: Out of physical memory");
      return 0;
    }

    if (map_page(heap_current_end, phys, PAGE_WRITABLE) != 0) {
      frame_free(phys);
      log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "Heap: Page mapping failed");
      return 0;
    }
    heap_current_end += PAGE_SIZE;
  }

  heap_block_t *new_block = (heap_block_t *)start_addr;
  new_block->size = (pages * PAGE_SIZE) - sizeof(heap_block_t);
  new_block->is_free = 1;
  new_block->next = 0;
  new_block->prev = heap_tail;

  if (heap_tail) {
    heap_tail->next = new_block;
  } else {
    heap_head = new_block;
  }
  heap_tail = new_block;

  return new_block;
}

/* Merges adjacent free blocks */
static void coalesce(heap_block_t *block) {
  if (block->next && block->next->is_free) {
    block->size += sizeof(heap_block_t) + block->next->size;
    block->next = block->next->next;
    if (block->next) {
      block->next->prev = block;
    } else {
      heap_tail = block;
    }
  }
  if (block->prev && block->prev->is_free) {
    heap_block_t *prev = block->prev;
    prev->size += sizeof(heap_block_t) + block->size;
    prev->next = block->next;
    if (block->next) {
      block->next->prev = prev;
    } else {
      heap_tail = prev;
    }
  }
}

/* Splits a block if there is enough excess space */
static void split_block(heap_block_t *block, unsigned int size) {
  if (block->size > size + sizeof(heap_block_t) + BLOCK_ALIGN) {
    heap_block_t *new_block =
        (heap_block_t *)((unsigned char *)block + sizeof(heap_block_t) + size);

    new_block->size = block->size - size - sizeof(heap_block_t);
    new_block->is_free = 1;
    new_block->next = block->next;
    new_block->prev = block;

    if (new_block->next) {
      new_block->next->prev = new_block;
    } else {
      heap_tail = new_block;
    }

    block->next = new_block;
    block->size = size;
  }
}

/* Shrinks the heap and releases physical frames if the tail is free and large
 * enough */
static void shrink_heap(void) {
  if (heap_tail && heap_tail->is_free) {
    unsigned int block_start = (unsigned int)heap_tail;

    /* If the tail block starts exactly on a page boundary, unmap it entirely */
    if ((block_start % PAGE_SIZE) == 0) {

      /* BUGFIX: Save the previous pointer before we unmap the page it lives on!
       */
      heap_block_t *prev = heap_tail->prev;

      while (heap_current_end > block_start) {
        heap_current_end -= PAGE_SIZE;
        unsigned int phys = unmap_page(heap_current_end);
        if (phys) {
          frame_free(phys);
        }
      }

      /* Use the saved pointer */
      if (prev) {
        heap_tail = prev;
        heap_tail->next = 0;
      } else {
        heap_head = 0;
        heap_tail = 0;
      }
    } else {
      /* Otherwise, only shrink excess complete pages */
      unsigned int new_end =
          ALIGN_UP(block_start + sizeof(heap_block_t) + BLOCK_ALIGN, PAGE_SIZE);

      if (new_end < heap_current_end) {
        unsigned int pages_to_free = (heap_current_end - new_end) / PAGE_SIZE;
        for (unsigned int i = 0; i < pages_to_free; i++) {
          heap_current_end -= PAGE_SIZE;
          unsigned int phys = unmap_page(heap_current_end);
          if (phys) {
            frame_free(phys);
          }
        }
        heap_tail->size = (new_end - block_start) - sizeof(heap_block_t);
      }
    }
  }
}

void *kmalloc(unsigned int size) {
  if (size == 0)
    return 0;

  size = ALIGN_UP(size, BLOCK_ALIGN);

  while (1) {
    heap_block_t *curr = heap_head;

    while (curr) {
      if (curr->is_free && curr->size >= size) {
        split_block(curr, size);
        curr->is_free = 0;
        return (void *)((unsigned char *)curr + sizeof(heap_block_t));
      }
      curr = curr->next;
    }

    heap_block_t *new_block = expand_heap(size + sizeof(heap_block_t));
    if (!new_block)
      return 0;

    coalesce(new_block);
  }
}

void kfree(void *ptr) {
  if (!ptr)
    return;

  heap_block_t *block =
      (heap_block_t *)((unsigned char *)ptr - sizeof(heap_block_t));
  block->is_free = 1;

  coalesce(block);
  shrink_heap();
}

void *krealloc(void *ptr, unsigned int size) {
  if (!ptr)
    return kmalloc(size);
  if (size == 0) {
    kfree(ptr);
    return 0;
  }

  heap_block_t *block =
      (heap_block_t *)((unsigned char *)ptr - sizeof(heap_block_t));
  unsigned int aligned_size = ALIGN_UP(size, BLOCK_ALIGN);

  if (block->size >= aligned_size) {
    split_block(block, aligned_size);
    return ptr;
  }

  void *new_ptr = kmalloc(size);
  if (!new_ptr)
    return 0;

  _memcpy(new_ptr, ptr, block->size);
  kfree(ptr);

  return new_ptr;
}

void heap_test(void) {
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "--- Starting Heap Tests ---");

  unsigned int initial_frames = memory_get_free_frames();

  // 1. Basic Allocation & Splitting
  void *ptr1 = kmalloc(100);
  void *ptr2 = kmalloc(200);
  void *ptr3 = kmalloc(300);

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Ptr1: 0x%x, Ptr2: 0x%x, Ptr3: 0x%x",
              (unsigned int)ptr1, (unsigned int)ptr2, (unsigned int)ptr3);

  // 2. Free Middle Block & Reuse
  kfree(ptr2);
  void *ptr4 = kmalloc(50); // Should reuse ptr2's space (splitting it)
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Ptr4 (should be = old Ptr2): 0x%x",
              (unsigned int)ptr4);

  // 3. Realloc Test
  ptr1 = krealloc(ptr1, 500);
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Ptr1 Realloc'd (likely new address): 0x%x", (unsigned int)ptr1);

  // 4. Large Allocation (Forces Heap Expansion)
  void *ptr_huge = kmalloc(8192); // Spans multiple pages
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "Huge Ptr: 0x%x",
              (unsigned int)ptr_huge);

  // 5. Cleanup & Frame Release Test
  kfree(ptr1);
  kfree(ptr3);
  kfree(ptr4);
  kfree(ptr_huge);

  unsigned int final_frames = memory_get_free_frames();
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "Initial free frames: %u, Final free frames: %u", initial_frames,
              final_frames);

  if (initial_frames == final_frames) {
    log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
                "Heap Test: Frame shrinking SUCCESS");
  } else {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "Heap Test: Memory leak detected");
  }

  log_message(LOG_INFO, LOG_OUTPUT_SERIAL, "--- Heap Tests Complete ---");
}
