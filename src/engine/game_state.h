#pragma once

#include <cute_alloc.h>
#include <cute_graphics.h>

#include "world.h"

typedef struct Platform Platform;

typedef struct GameState {
  Platform* platform;
  CF_Arena* scratch_arena;

  CF_Canvas canvas;           // The main game canvas
  CF_Canvas composite_canvas; // Composited game+lighting for ImGui display
  int composite_w;            // Current composite canvas width (0 = not created)
  int composite_h;            // Current composite canvas height

  bool debug_mode;

  World world;
} GameState;

extern GameState* state;
