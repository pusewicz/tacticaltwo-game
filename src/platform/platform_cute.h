#pragma once

#include <stdbool.h>

typedef struct Input Input;
typedef struct Platform Platform;

typedef void (*GameInitFunction)(Platform* platform);
typedef bool (*GameUpdateFunction)(void);
typedef void (*GameRenderFunction)(void);
typedef void (*GameShutdownFunction)(void);
typedef void* (*GameStateFunction)(void);
typedef void (*GameHotReloadFunction)(void* game_state);

typedef struct GameLibrary {
  void* library;
  const char* path;

  GameInitFunction init;
  GameUpdateFunction update;
  GameRenderFunction render;
  GameShutdownFunction shutdown;
  GameStateFunction state;
  GameHotReloadFunction hot_reload;

  bool ok;
} GameLibrary;

void platform_init(int argc, char* argv[]);
void platform_shutdown(void);

int platform_get_page_size(void);

void platform_begin_frame(void);
void platform_end_frame(void);

GameLibrary platform_load_game_library(void);
void platform_unload_game_library(GameLibrary* game_library);
bool platform_game_library_has_changed(GameLibrary* game_library);
