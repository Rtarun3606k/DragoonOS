#ifndef SERIAL_H
#define SERIAL_H

void serial_configure_baud_rate(unsigned short com, unsigned short divisor);
void serial_initialize(void);
void serial_write(const char *buf, unsigned int len);

#endif
