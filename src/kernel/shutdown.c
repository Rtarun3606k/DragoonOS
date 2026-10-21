#include "shutdown.h"
#include "io.h"

void kernel_halt(void) {
  __asm__ volatile("cli");

  for (;;) {
    __asm__ volatile("hlt");
  }
}

void kernel_shutdown(void) {
  __asm__ volatile("cli");
outw(0xB004, 0x2000);
  /* Stop execution safely if shutdown isn't supported. */
  for (;;) {
    __asm__ volatile("hlt");
  }
}
