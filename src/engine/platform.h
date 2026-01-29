#pragma once

typedef struct Platform {
  int (*get_system_page_size)(void);
} Platform;
