
#include "serial.h"
#include "io.h"

#define SERIAL_COM1_BASE 0x3F8

#define SERIAL_DATA_PORT(base) (base)
#define SERIAL_FIFO_COMMAND_PORT(base) ((base) + 2)
#define SERIAL_LINE_COMMAND_PORT(base) ((base) + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) ((base) + 4)
#define SERIAL_LINE_STATUS_PORT(base) ((base) + 5)

#define SERIAL_LINE_STATUS_TRANSMITTER_EMPTY 0x20

#define SERIAL_LINE_ENABLE_DLAB 0x80

#define SERIAL_DATA_BITS_8 0x03

void serial_configure_baud_rate(unsigned short com, unsigned short divisor) {
  outb(SERIAL_LINE_COMMAND_PORT(com), SERIAL_LINE_ENABLE_DLAB);
  outb(SERIAL_DATA_PORT(com), (divisor >> 8) & 0x00FF);

  outb(SERIAL_DATA_PORT(com), divisor & 0x00FF);
}

void serial_configure_line(unsigned short com) {
  outb(SERIAL_LINE_COMMAND_PORT(com), SERIAL_DATA_BITS_8);
}

void serial_initialize(void) {
  /* Disable serial interrupts. */
  outb(SERIAL_COM1_BASE + 1, 0x00);

  /* Configure baud rate: 115200 / 3 = 38400. */
  serial_configure_baud_rate(SERIAL_COM1_BASE, 3);

  /* Configure 8 data bits, no parity, 1 stop bit. */
  serial_configure_line(SERIAL_COM1_BASE);

  /* Enable FIFO and clear its buffers. */
  outb(SERIAL_FIFO_COMMAND_PORT(SERIAL_COM1_BASE), 0xC7);

  /* Enable DTR, RTS, and OUT2. */
  outb(SERIAL_MODEM_COMMAND_PORT(SERIAL_COM1_BASE), 0x0B);
}

static int serial_is_transmit_empty(void) {
  return (inb(SERIAL_LINE_STATUS_PORT(SERIAL_COM1_BASE)) &
          SERIAL_LINE_STATUS_TRANSMITTER_EMPTY) != 0;
}

void serial_write_char(char c) {
  while (!serial_is_transmit_empty()) {
    /* Wait until the transmitter can accept a byte. */
  }

  outb(SERIAL_DATA_PORT(SERIAL_COM1_BASE), (unsigned char)c);
}

void serial_write(const char *buf, unsigned int len) {
  unsigned int i;

  for (i = 0; i < len; i++) {
    serial_write_char(buf[i]);
  }
}
