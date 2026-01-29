#pragma once

typedef struct Platform Platform;
typedef struct CF_Arena CF_Arena;

typedef struct GameState {
  Platform* platform;
  CF_Arena* scratch_arena;

  bool debug_mode;
} GameState;

extern GameState* state;
