#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

int fb_write(const char *buf, unsigned int len);
void fb_backspace(void);
void fb_clear(void);
void fb_disable_cursor(void);

#endif
