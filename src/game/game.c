#include "game.h"

#include <cute_alloc.h>
#include <cute_app.h>
#include <cute_c_runtime.h>
#include <cute_defines.h>
#include <cute_draw.h>
#include <cute_input.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <cute_time.h>
#include <dcimgui.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../config/config.h"
#include "../engine/asset.h"
#include "../engine/game_state.h"
#include "../engine/platform.h"

GameState* state = nullptr;

EXPORT void game_init(Platform* platform) {
  state = calloc(1, sizeof(GameState));
  CF_ASSERT(state != nullptr);

  state->platform = platform;
  state->scratch_arena = malloc(sizeof(CF_Arena));
  *state->scratch_arena = cf_make_arena(_Alignof(void*), CF_MB * 4);

  asset_load_sprite("assets/sprites/player.ase", &state->player_sprite);

  cf_app_init_imgui();
}

bool game_update(void) {
  cf_arena_reset(state->scratch_arena);

  if (cf_key_just_pressed(CF_KEY_G)) {
    state->debug_mode = !state->debug_mode;
  }

  cf_sprite_update(&state->player_sprite);

  return true;
}

void game_render(void) {
  cf_draw_scale(CANVAS_SCALE, CANVAS_SCALE);
  cf_draw_sprite(&state->player_sprite);
}

EXPORT void game_shutdown(void) {
  free(state->scratch_arena);
  free(state);
}

EXPORT void* game_state(void) { return state; }

EXPORT void game_hot_reload(void* game_state) {
  state = (GameState*)game_state;
}
