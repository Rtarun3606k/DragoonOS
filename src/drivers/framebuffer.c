#include "framebuffer.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/*
 * VGA text-mode colors.  The attribute byte is
 *   ((background & 0x0F) << 4) | (foreground & 0x0F)
 * so a background value >= 8 sets bit 7 of the attribute byte, which is the
 * hardware blink flag in standard text mode: every character using that
 * background blinks on and off.  Keep the background in 0..7 (black here).
 */
#define FRAME_BUFFER_BLACK 0
#define FRAME_BUFFER_GREEN 2

#define FB_COMMAND_PORT 0x3D4
#define FB_DATA_PORT 0x3D5
#define FB_HIGH_BYTE_COMMAND 14
#define FB_LOW_BYTE_COMMAND 15

/* VGA Attribute Controller (used to turn off hardware blink). */
#define VGA_INPUT_STATUS_PORT 0x3DA
#define VGA_AC_INDEX_PORT 0x3C0
#define VGA_AC_DATA_PORT 0x3C1
#define VGA_AC_MODE_CONTROL 0x10
#define VGA_AC_PAS 0x20
#define VGA_AC_BLINK_ENABLE 0x08

static volatile char *fb = (char *)0xC00B8000;
static unsigned int cursor_position = 0;

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

void fb_write_string(const char *str, unsigned int row, unsigned char fg,
                     unsigned char bg) {

  unsigned pos = row * VGA_WIDTH * 2;

  while (*str != '\0') {
    fb_write_cell(pos, *str, fg, bg);
    pos += 2;
    str++;
  }
}

// crsore moment mouse moment
void fb_move_cursor(unsigned short pos) {
  outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
  outb(FB_DATA_PORT, (pos >> 8) & 0x00FF);

  outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
  outb(FB_DATA_PORT, pos & 0x00FF);
}

/*
 * Clear bit 3 (Blink Enable) of the Attribute Controller Mode Control
 * register.  Bochs (and real VGA hardware) enables blink by default in text
 * mode, so with this bit set any attribute byte with bit 7 set blinks.
 *
 * The Attribute Controller uses an index/data flip-flop that is reset by
 * reading the Input Status Register 0x3DA.  The index is written with the
 * PAS bit (0x20) set so the video output stays enabled.
 */
static void fb_disable_blink(void) {
  unsigned char mode;

  (void)inb(VGA_INPUT_STATUS_PORT);
  outb(VGA_AC_INDEX_PORT, VGA_AC_MODE_CONTROL);
  mode = inb(VGA_AC_DATA_PORT);

  (void)inb(VGA_INPUT_STATUS_PORT);
  outb(VGA_AC_INDEX_PORT, VGA_AC_MODE_CONTROL | VGA_AC_PAS);
  outb(VGA_AC_INDEX_PORT, (unsigned char)(mode & ~VGA_AC_BLINK_ENABLE));
}

static void fb_scroll(void) {
  unsigned int row;
  unsigned int col;

  for (row = 1; row < VGA_HEIGHT; row++) {
    for (col = 0; col < VGA_WIDTH; col++) {
      unsigned int src = (row * VGA_WIDTH + col) * 2;
      unsigned int dst = ((row - 1) * VGA_WIDTH + col) * 2;

      fb[dst] = fb[src];
      fb[dst + 1] = fb[src + 1];
    }
  }

  for (col = 0; col < VGA_WIDTH; col++) {
    fb_write_cell(((VGA_HEIGHT - 1) * VGA_WIDTH + col) * 2, ' ',
                  FRAME_BUFFER_GREEN, FRAME_BUFFER_BLACK);
  }

  cursor_position -= VGA_WIDTH;
}

int fb_write(const char *buf, unsigned int len) {
  unsigned int i;

  for (i = 0; i < len; i++) {
    char c = buf[i];

    if (c == '\n') {
      cursor_position = ((cursor_position / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else if (c == '\r') {
      cursor_position -= cursor_position % VGA_WIDTH;
    } else {
      fb_write_cell(cursor_position * 2, c, FRAME_BUFFER_GREEN,
                    FRAME_BUFFER_BLACK);

      cursor_position++;
    }

    if (cursor_position >= VGA_WIDTH * VGA_HEIGHT) {
      fb_scroll();
    }
  }

  fb_move_cursor(cursor_position);
  return (int)len;
}

void fb_backspace(void) {

  if (cursor_position == 0) {
    return;
  }

  cursor_position--;

  fb_write_cell(cursor_position * 2, ' ', FRAME_BUFFER_GREEN,
                FRAME_BUFFER_BLACK);

  fb_move_cursor(cursor_position);
}

void fb_clear(void) {
  unsigned int i = 0;

  /* Make sure a stale blink bit cannot make the cleared screen blink. */
  fb_disable_blink();

  for (i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
    fb_write_cell(i * 2, ' ', FRAME_BUFFER_GREEN, FRAME_BUFFER_BLACK);
  }
  cursor_position = 0;
  fb_move_cursor(cursor_position);
}


void fb_disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
