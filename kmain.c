#include "io.h"

#define Frame_Buffer_Green 2
#define Frame_Buffer_Dark_Grey 8
#define VGA_Width 80

// Cursor moment
#define FRAME_BUFFER_COMMAND_PORT 0x3D4
#define FRAME_BUFFER_DATA_PORT 0x3d5

#define FB_HIGH_BYTE_COMMAND 14
#define FB_LOW_BYTE_COMMAND 15

// assign a beginig frame position
static char *fb = (char *)0x000B8000;

// frame buffer
/**
 * Writes a character with the given foreground and background
 * colors to position i in the framebuffer.
 *
 * @param i  The byte offset in the framebuffer
 * @param c  The character
 * @param fg The foreground color
 * @param bg The background color
 */
void fb_write_cell(unsigned int position, unsigned char character,
                   unsigned char fore_Ground, unsigned char background) {

  fb[position] = character;
  fb[position + 1] = ((background & 0x0F) << 4) | (fore_Ground & 0x0F);
}

// writes a string to the os screen on boot
void fb_write_string(const char *str, unsigned int row, unsigned char fg,
                     unsigned char bg) {

  unsigned pos = row * VGA_Width * 2;

  while (*str != '\0') {
    fb_write_cell(pos, *str, fg, bg);
    pos += 2;
    str++;
  }
}

// crsore moment mouse moment
void fb_move_cursor(unsigned short pos) {
  outb(FRAME_BUFFER_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
  outb(FRAME_BUFFER_DATA_PORT, (pos >> 8) & 0x00FF);

  outb(FRAME_BUFFER_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
  outb(FRAME_BUFFER_DATA_PORT, pos & 0x00FF);
}

void kmain(void) {

  fb_write_cell(0, 'D', Frame_Buffer_Green, Frame_Buffer_Dark_Grey);
  fb_write_string("Hello from my OS!", 0, Frame_Buffer_Green,
                  Frame_Buffer_Dark_Grey);
  fb_move_cursor(240);

  for (;;) {
    __asm__ volatile("hlt");
  }
}
