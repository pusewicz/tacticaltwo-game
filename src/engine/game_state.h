#pragma once

#include <cute_sprite.h>

typedef struct Platform Platform;
typedef struct CF_Arena CF_Arena;

typedef struct GameState {
  Platform* platform;
  CF_Arena* scratch_arena;

  bool debug_mode;

  CF_Sprite player_sprite;
} GameState;

extern GameState* state;
