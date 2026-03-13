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
#include <stdlib.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/log.h"
#include "../engine/platform.h"
#include "ldtk.h"
#include "night.h"
#include "rain.h"
#include "world.h"

GameState* state = nullptr;

// Handles hot-reload for all game shaders (overrides rain.c's callback)
static void on_shader_changed(const char* path,
                               [[maybe_unused]] void* udata) {
  if (state->world.night.initialized) {
    if (cf_shader_reload(&state->world.night.shader)) {
      log_info("game", "Night shader reloaded: %s", path);
    }
  }
  if (state->world.rain.initialized) {
    cf_shader_reload(&state->world.rain.shader);
  }
}

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

  // Set up projection for the game canvas
  cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH * CANVAS_SCALE,
                                 CANVAS_HEIGHT * CANVAS_SCALE));

  cf_shader_directory("/assets/shaders");

  init_world();
  night_init();

  // Global shader hot-reload callback (supersedes rain.c's callback)
  cf_shader_on_changed(on_shader_changed, nullptr);

  cf_app_init_imgui();
}

bool game_update(void) {
  cf_arena_reset(state->scratch_arena);

  if (cf_key_just_pressed(CF_KEY_G)) {
    state->debug_mode = !state->debug_mode;
  }

  update_world(CF_DELTA_TIME);

  if (state->debug_mode) {
    ImGui_Begin("Debug", &state->debug_mode, 0);

    int p = state->world.player;
    if (p != ENTITY_NONE) {
      Entity* player = &state->world.entities[p];

      ImGui_SeparatorText("Collider");
      ImGui_SliderFloat("half_w", &player->collider.half_size.x, 1.0f, 32.0f);
      ImGui_SliderFloat("half_h", &player->collider.half_size.y, 1.0f, 48.0f);
      ImGui_SliderFloat("offset_x", &player->collider.offset.x, -48.0f, 48.0f);
      ImGui_SliderFloat("offset_y", &player->collider.offset.y, -48.0f, 48.0f);
      ImGui_Text("grounded: %s", player->collider.grounded ? "true" : "false");

      ImGui_SeparatorText("Transform");
      ImGui_Text("pos: %.1f, %.1f", (double)player->transform.position.x,
                 (double)player->transform.position.y);
      ImGui_Text("vel: %.1f, %.1f", (double)player->velocity.value.x,
                 (double)player->velocity.value.y);
    }

    LdtkMap* map = &state->world.map;
    if (map->loaded) {
      LdtkLevel* lvl = &map->levels[map->active_level];
      ImGui_SeparatorText("Level");
      ImGui_Text("name: %s", lvl->identifier);
      ImGui_Text("pos: %d, %d  size: %dx%d", lvl->x, lvl->y, lvl->width,
                 lvl->height);
      ImGui_Text("grid: %dx%d", lvl->grid_width, lvl->grid_height);
    }

    ImGui_SeparatorText("Rain");
    ImGui_SliderFloat("intensity", &state->world.rain.intensity, 0.0f, 1.0f);
    ImGui_SliderFloat("density", &state->world.rain.density, 0.0f, 1.0f);
    ImGui_SliderFloat("alpha", &state->world.rain.alpha, 0.0f, 1.0f);
    ImGui_SliderFloat("speed", &state->world.rain.speed, 0.1f, 5.0f);
    ImGui_SliderFloat("wind", &state->world.rain.wind, -1.0f, 1.0f);

    ImGui_SeparatorText("Rain Splash");
    ImGui_SliderFloat("cell size", &state->world.rain.splash_cell_size, 4.0f, 30.0f);
    ImGui_SliderFloat("rate", &state->world.rain.splash_rate, 1.0f, 20.0f);
    ImGui_SliderFloat("life", &state->world.rain.splash_life, 0.05f, 1.0f);
    ImGui_SliderFloat("splash speed", &state->world.rain.splash_speed, 2.0f, 40.0f);
    ImGui_SliderFloat("gravity", &state->world.rain.splash_gravity, 5.0f, 80.0f);

    ImGui_SeparatorText("Night");
    ImGui_SliderFloat("night intensity", &state->world.night.intensity, 0.0f, 1.0f);
    ImGui_SliderFloat("desaturation", &state->world.night.desaturation, 0.0f, 1.0f);
    ImGui_ColorEdit3("night tint", state->world.night.tint, 0);

    ImGui_End();
  }

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

    night_push();
    cf_draw_canvas(state->canvas, cf_v2(0, 0), dest);
    night_pop();

    // Restore projection for the next frame
    cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
  }

  cf_draw_pop_filter();
}

void game_shutdown(void) {
  shutdown_world();
  night_shutdown();
  cf_destroy_canvas(state->canvas);
  cf_destroy_arena(state->scratch_arena);
  free(state->scratch_arena);
  free(state);
}

void* game_state(void) { return state; }

void game_hot_reload(void* game_state) {
  state = (GameState*)game_state;
  world_hot_reload();
}
