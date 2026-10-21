#ifndef LOG_H
#define LOG_H

typedef enum {

  LOG_DEBUG,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL,
  LOG_INFO,

} log_level_t;

typedef enum {
  LOG_OUTPUT_FRAME_BUFFER,
  LOG_OUTPUT_SERIAL,
} log_output_t; // output type

void log_message(log_level_t level, log_output_t output, const char *format,
                 ...);

#endif
