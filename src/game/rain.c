// rain.c - Procedural rain storm effect
//
// Uses a 2D collision mask for rain occlusion (solid tiles block rain)
// and a 1D height map for splash positioning (topmost solid per column).

#include "rain.h"

#include <cute_draw.h>
#include <cute_graphics.h>
#include <cute_math.h>
#include <stdint.h>
#include <string.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/log.h"
#include "ldtk.h"
#include "world.h"

// =============================================================================
// Texture Generation
// =============================================================================

static void build_textures(RainState* rain, LdtkLevel* level) {
  if (!level->int_grid || level->grid_width <= 0 || level->grid_height <= 0) {
    return;
  }

  int w = level->grid_width;
  int h = level->grid_height;

  // --- 2D collision mask (grid_width x grid_height) ---
  {
    int pixel_bytes = w * h * 4;
    uint8_t* buf =
        (uint8_t*)cf_arena_alloc(state->scratch_arena, pixel_bytes);

    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        int idx      = (row * w + col) * 4;
        bool solid   = level->int_grid[row * w + col] > 0;
        buf[idx + 0] = solid ? 255 : 0;
        buf[idx + 1] = 0;
        buf[idx + 2] = 0;
        buf[idx + 3] = 255;
      }
    }

    if (rain->mask_width != w || rain->mask_height != h) {
      if (rain->mask_width > 0) {
        cf_destroy_texture(rain->collision_mask);
      }
      CF_TextureParams params = cf_texture_defaults(w, h);
      params.filter           = CF_FILTER_NEAREST;
      params.wrap_u           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      params.wrap_v           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      rain->collision_mask    = cf_make_texture(params);
      rain->mask_width        = w;
      rain->mask_height       = h;
    }

    cf_texture_update(rain->collision_mask, buf, pixel_bytes);
  }

  // --- 1D height map (grid_width x 1) ---
  // R channel = grid row of topmost solid tile (255 = no ground)
  {
    int pixel_bytes = w * 4;
    uint8_t* buf =
        (uint8_t*)cf_arena_alloc(state->scratch_arena, pixel_bytes);

    for (int col = 0; col < w; col++) {
      uint8_t ground_row = 255;
      for (int row = 0; row < h; row++) {
        if (level->int_grid[row * w + col] > 0) {
          ground_row = (uint8_t)(row < 255 ? row : 254);
          break;
        }
      }
      buf[col * 4 + 0] = ground_row;
      buf[col * 4 + 1] = 0;
      buf[col * 4 + 2] = 0;
      buf[col * 4 + 3] = 255;
    }

    // Reuse mask_width to track — height map always matches collision mask width
    static bool height_map_created = false;
    if (!height_map_created) {
      CF_TextureParams params = cf_texture_defaults(w, 1);
      params.filter           = CF_FILTER_NEAREST;
      params.wrap_u           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      params.wrap_v           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      rain->height_map        = cf_make_texture(params);
      height_map_created      = true;
    }

    cf_texture_update(rain->height_map, buf, pixel_bytes);
  }

  log_info("rain", "Built collision mask %dx%d + height map %d cols", w, h, w);
}

// =============================================================================
// Public API
// =============================================================================

void rain_init(void) {
  RainState* rain = &state->world.rain;
  memset(rain, 0, sizeof(RainState));

  rain->shader    = cf_make_draw_shader("rain.shd");
  rain->intensity = 1.0f;
  rain->density   = 1.0f;
  rain->alpha     = 1.0f;
  rain->speed     = 5.0f;
  rain->wind      = 0.05f;

  LdtkMap* map = &state->world.map;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    build_textures(rain, &map->levels[map->active_level]);
  }

  rain->splash_cell_size = 24.0f;
  rain->splash_rate      = 4.0f;
  rain->splash_life      = 1.0f;
  rain->splash_speed     = 20.0f;
  rain->splash_gravity   = 20.0f;
  rain->initialized = true;
  rain->enabled     = false;

  log_info("rain", "Rain system initialized (intensity=%.1f)",
           (double)rain->intensity);
}

void rain_rebuild_height_map(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized) {
    return;
  }

  LdtkMap* map = &state->world.map;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    build_textures(rain, &map->levels[map->active_level]);
  }
}

void rain_render(float dt) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized || rain->intensity <= 0.001f) {
    return;
  }

  rain->time += dt;

  LdtkMap* map     = &state->world.map;
  LdtkLevel* level = nullptr;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    level = &map->levels[map->active_level];
  }

  float hw = (float)CANVAS_WIDTH / 2.0f;
  float hh = (float)CANVAS_HEIGHT / 2.0f;
  CF_Aabb fullscreen = {.min = cf_v2(-hw, -hh), .max = cf_v2(hw, hh)};

  // Atmospheric tint
  {
    float tint_alpha = 0.35f * rain->intensity;
    cf_draw_push_color(
        cf_make_color_rgba_f(0.08f, 0.06f, 0.18f, tint_alpha));
    cf_draw_box_fill(fullscreen, 0);
    cf_draw_pop_color();
  }

  // Rain shader overlay
  if (rain->mask_width > 0 && level != nullptr) {
    cf_draw_push_shader(rain->shader);
    cf_draw_set_uniform_float("u_time", rain->time);
    cf_draw_set_uniform_float("u_intensity", rain->intensity);
    cf_draw_set_uniform_float("u_density", rain->density);
    cf_draw_set_uniform_float("u_alpha", rain->alpha);
    cf_draw_set_uniform_float("u_speed", rain->speed);
    cf_draw_set_uniform_float("u_wind", rain->wind);
    cf_draw_set_uniform_v2("u_camera", state->world.camera);
    cf_draw_set_uniform_v2("u_canvas_size",
                           cf_v2((float)CANVAS_WIDTH, (float)CANVAS_HEIGHT));
    cf_draw_set_uniform_v2(
        "u_level_size",
        cf_v2((float)level->width, (float)level->height));
    cf_draw_set_uniform_float("u_grid_size", (float)LDTK_GRID_SIZE);
    cf_draw_set_uniform_float("u_grid_width", (float)rain->mask_width);
    cf_draw_set_uniform_float("u_grid_height", (float)rain->mask_height);
    cf_draw_set_uniform_float("u_splash_cell_size", rain->splash_cell_size);
    cf_draw_set_uniform_float("u_splash_rate", rain->splash_rate);
    cf_draw_set_uniform_float("u_splash_life", rain->splash_life);
    cf_draw_set_uniform_float("u_splash_speed", rain->splash_speed);
    cf_draw_set_uniform_float("u_splash_gravity", rain->splash_gravity);
    cf_draw_set_texture("collision_tex", rain->collision_mask);
    cf_draw_set_texture("height_tex", rain->height_map);

    cf_draw_push_color(cf_color_white());
    cf_draw_box_fill(fullscreen, 0);
    cf_draw_pop_color();

    cf_draw_pop_shader();
  }
}

void rain_shutdown(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized) {
    return;
  }

  if (rain->mask_width > 0) {
    cf_destroy_texture(rain->collision_mask);
    cf_destroy_texture(rain->height_map);
  }
  cf_destroy_shader(rain->shader);
  rain->initialized = false;

  log_info("rain", "Rain system shut down");
}
