#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H
#include <stdint.h>

uint8_t inb(uint16_t port);

/**
 * Sends a byte to an I/O port.
 *
 * @param port The I/O port
 * @param data The byte to send
 */
void outb(unsigned short port, unsigned char data);
unsigned char inb(unsigned short port);
void outw(unsigned short port, unsigned short data); //shutdown

#endif
