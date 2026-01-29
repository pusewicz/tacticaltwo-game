#include "log.h"

#include <SDL3/SDL_log.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void log_init(void) {
#ifdef DEBUG
  SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#endif
  log_debug("log", "Logging initialized.");
}

void log_write(LogLevel level, const char* tag, const char* file, int line,
               const char* fmt, ...) {
  // Map LogLevel to SDL_LogPriority
  static const SDL_LogPriority priorities[] = {
      [LOG_LEVEL_DEBUG] = SDL_LOG_PRIORITY_DEBUG,
      [LOG_LEVEL_INFO] = SDL_LOG_PRIORITY_INFO,
      [LOG_LEVEL_WARN] = SDL_LOG_PRIORITY_WARN,
      [LOG_LEVEL_ERROR] = SDL_LOG_PRIORITY_ERROR,
      [LOG_LEVEL_FATAL] = SDL_LOG_PRIORITY_CRITICAL,
  };

  // Strip source directory prefix from file path
#ifdef LOG_SOURCE_DIR
  static const size_t prefix_len = sizeof(LOG_SOURCE_DIR) - 1;
  if (strncmp(file, LOG_SOURCE_DIR, prefix_len) == 0) {
    file += prefix_len;
  }
#endif

  // Format user message
  char user_msg[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(user_msg, sizeof(user_msg), fmt, args);
  va_end(args);

  // Format final message with file:line and optional tag
  char final_msg[1280];
  if (tag) {
    snprintf(final_msg, sizeof(final_msg), "[%s] %s:%d: %s", tag, file, line,
             user_msg);
  } else {
    snprintf(final_msg, sizeof(final_msg), "%s:%d: %s", file, line, user_msg);
  }

  SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priorities[level], "%s",
                 final_msg);
}
