#include "game.h"

#include <cute_alloc.h>
#include <cute_app.h>
#include <cute_c_runtime.h>
#include <cute_defines.h>
#include <cute_draw.h>
#include <cute_graphics.h>
#include <cute_input.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <cute_time.h>
#include <dcimgui.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/platform.h"
#include "world.h"

GameState* state = nullptr;

static CF_V2 calculate_dest_size(CF_V2 game, CF_V2 window) {
  float game_aspect   = game.x / game.y;
  float window_aspect = window.x / window.y;
  float dest_w;
  float dest_h;

  if (window_aspect > game_aspect) {
    // Window is wider - pillarbox (black bars on sides).
    dest_h = window.y;
    dest_w = dest_h * game_aspect;
  } else {
    // Window is taller - letterbox (black bars on top/bottom).
    dest_w = window.x;
    dest_h = dest_w / game_aspect;
  }

  return cf_v2(dest_w, dest_h);
}

void game_init(Platform* platform) {
  state = calloc(1, sizeof(GameState));
  CF_ASSERT(state != nullptr);

  state->platform       = platform;
  state->scratch_arena  = malloc(sizeof(CF_Arena));
  *state->scratch_arena = cf_make_arena(_Alignof(void*), CF_MB * 4);
  state->canvas =
      cf_make_canvas(cf_canvas_defaults(CANVAS_WIDTH, CANVAS_HEIGHT));

  // Set up projection for the game vcanvas
  cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH * CANVAS_SCALE,
                                 CANVAS_HEIGHT * CANVAS_SCALE));

  init_world();

  cf_app_init_imgui();
}

bool game_update(void) {
  cf_arena_reset(state->scratch_arena);

  if (cf_key_just_pressed(CF_KEY_G)) {
    state->debug_mode = !state->debug_mode;
  }

  update_world(CF_DELTA_TIME);

  return true;
}

void game_render(void) {
  cf_draw_push_filter(CF_DRAW_FILTER_NEAREST);

  // Render to the game canvas
  {
    // Cornflower blue (6495ED) background
    cf_clear_color(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    cf_clear_canvas(state->canvas);

    render_world();

    // Draw game content to the canvas.
    cf_render_to(state->canvas, true);
  }

  // Render canvas to the window with aspect ratio correction
  {
    int window_w = cf_app_get_width();
    int window_h = cf_app_get_height();

    cf_clear_color(0, 0, 0, 1.0f); // Black bars

    cf_app_set_canvas_size(window_w, window_h);
    CF_V2 dest = calculate_dest_size(cf_v2(CANVAS_WIDTH, CANVAS_HEIGHT),
                                     cf_v2((float)window_w, (float)window_h));
    cf_draw_projection(cf_ortho_2d(0, 0, (float)window_w, (float)window_h));
    cf_draw_canvas(state->canvas, cf_v2(0, 0), dest);

    // Restore pojection for the next frame
    cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
  }

  cf_draw_pop_filter();
}

void game_shutdown(void) {
  shutdown_world();
  free(state->scratch_arena);
  free(state);
}

void* game_state(void) { return state; }

void game_hot_reload(void* game_state) {
  state = (GameState*)game_state;
  world_hot_reload();
}
