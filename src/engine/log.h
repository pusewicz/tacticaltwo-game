#pragma once

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL
} LogLevel;

// Core function (called by macros)
__attribute__((format(printf, 5, 6))) void log_write(LogLevel level,
                                                     const char* tag,
                                                     const char* file, int line,
                                                     const char* fmt, ...);

// Initialize logging (sets up SDL log priorities in debug builds)
void log_init(void);

// Convenience macros with file/line capture
#define log_debug(tag, fmt, ...)                                               \
  log_write(LOG_LEVEL_DEBUG, tag, __FILE__, __LINE__,                          \
            fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_info(tag, fmt, ...)                                                \
  log_write(LOG_LEVEL_INFO, tag, __FILE__, __LINE__,                           \
            fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_warn(tag, fmt, ...)                                                \
  log_write(LOG_LEVEL_WARN, tag, __FILE__, __LINE__,                           \
            fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_error(tag, fmt, ...)                                               \
  log_write(LOG_LEVEL_ERROR, tag, __FILE__, __LINE__,                          \
            fmt __VA_OPT__(, ) __VA_ARGS__)
#define log_fatal(tag, fmt, ...)                                               \
  log_write(LOG_LEVEL_FATAL, tag, __FILE__, __LINE__,                          \
            fmt __VA_OPT__(, ) __VA_ARGS__)
