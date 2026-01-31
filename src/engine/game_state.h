#pragma once

#include <cute_alloc.h>

#include "world.h"

typedef struct Platform Platform;

typedef struct GameState {
  Platform* platform;
  CF_Arena* scratch_arena;

  bool debug_mode;

  World world;
} GameState;

extern GameState* state;
