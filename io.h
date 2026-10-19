#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

/**
 * Sends a byte to an I/O port.
 *
 * @param port The I/O port
 * @param data The byte to send
 */
void outb(unsigned short port, unsigned char data);

#endif
