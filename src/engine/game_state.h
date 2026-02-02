#pragma once

#include <cute_alloc.h>
#include <cute_graphics.h>

#include "world.h"

typedef struct Platform Platform;

typedef struct GameState {
  Platform* platform;
  CF_Arena* scratch_arena;

  CF_Canvas canvas; // The main game canvas

  bool debug_mode;

  World world;
} GameState;

extern GameState* state;
