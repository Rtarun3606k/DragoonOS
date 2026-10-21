#include "log.h"
#include "framebuffer.h"
#include "serial.h"

#include <stdarg.h>

#define LOG_BUFFER_SIZE 1024

static const char *log_level_name(log_level_t level) {
  switch (level) {
  case LOG_DEBUG:
    return "DEBUG";
  case LOG_INFO:
    return "INFO";
  case LOG_WARN:
    return "WARN";
  case LOG_ERROR:
    return "ERROR";
  case LOG_FATAL:
    return "FATAL";
  default:
    return "UNKNOWN";
  }
}

static void append_char(char *buf, unsigned int *len, char c) {
  if (*len < LOG_BUFFER_SIZE - 1) {
    buf[*len] = c;
    (*len)++;
  }
}

static void append_string(char *buf, unsigned int *len, const char *str) {
  if (str == 0)
    str = "(null)";

  while (*str != '\0') {
    append_char(buf, len, *str);
    str++;
  }
}

static void append_unsigned(char *buf, unsigned int *len, unsigned int value,
                            unsigned int base) {
  char digits[32];
  unsigned int count = 0;

  do {
    unsigned int digit = value % base;

    if (digit < 10)
      digits[count++] = '0' + digit;
    else
      digits[count++] = 'a' + (digit - 10);

    value /= base;
  } while (value != 0);

  while (count > 0)
    append_char(buf, len, digits[--count]);
}

static void format_message(char *buf, unsigned int *len, const char *format,
                           va_list args) {
  while (*format != '\0') {
    if (*format != '%') {
      append_char(buf, len, *format++);
      continue;
    }

    format++;

    if (*format == '\0') {
      append_char(buf, len, '%');
      break;
    }

    switch (*format) {
    case 's':
      append_string(buf, len, va_arg(args, const char *));
      break;

    case 'c':
      append_char(buf, len, (char)va_arg(args, int));
      break;

    case 'd': {
      int value = va_arg(args, int);
      unsigned int magnitude;

      if (value < 0) {
        append_char(buf, len, '-');
        magnitude = 0u - (unsigned int)value;
      } else {
        magnitude = (unsigned int)value;
      }

      append_unsigned(buf, len, magnitude, 10);
      break;
    }

    case 'u':
      append_unsigned(buf, len, va_arg(args, unsigned int), 10);
      break;

    case 'x':
      append_unsigned(buf, len, va_arg(args, unsigned int), 16);
      break;

    case '%':
      append_char(buf, len, '%');
      break;

    default:
      append_char(buf, len, '%');
      append_char(buf, len, *format);
      break;
    }

    format++;
  }
}

void log_message(log_level_t level, log_output_t output, const char *format,
                 ...) {
  char buf[LOG_BUFFER_SIZE];
  unsigned int len = 0;
  const char *name = log_level_name(level);
  va_list args;

  append_char(buf, &len, '[');
  append_string(buf, &len, name);
  append_string(buf, &len, "] ");

  va_start(args, format);
  format_message(buf, &len, format, args);
  va_end(args);

  if (output == LOG_OUTPUT_FRAME_BUFFER) {
    append_char(buf, &len, '\n');
    fb_write(buf, len);
  } else if (output == LOG_OUTPUT_SERIAL) {
    append_char(buf, &len, '\r');
    append_char(buf, &len, '\n');
    serial_write(buf, len);
  }
}
