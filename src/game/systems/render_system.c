// render_system.c - Sprite rendering and debug overlay systems
//
// Draws all entities with Sprite and Transform components.
// Debug mode renders collider AABBs and nearby solid tiles.

#include <cute_color.h>
#include <cute_draw.h>
#include <cute_math.h>
#include <math.h>

#include "../../engine/game_state.h"
#include "../ldtk.h"
#include "../world.h"
#include "systems.h"

// =============================================================================
// Coordinate helpers (same as physics_system.c)
// =============================================================================

static int debug_cf_x_to_grid(float cf_x, int level_width) {
  float ldtk_x = cf_x + (float)level_width / 2.0f;
  return (int)floorf(ldtk_x / (float)LDTK_GRID_SIZE);
}

static int debug_cf_y_to_grid(float cf_y, int level_height) {
  float ldtk_y = (float)level_height / 2.0f - cf_y;
  return (int)floorf(ldtk_y / (float)LDTK_GRID_SIZE);
}

static CF_Aabb debug_grid_cell_aabb(int gx, int gy, int level_width,
                                    int level_height) {
  float half_w = (float)level_width / 2.0f;
  float half_h = (float)level_height / 2.0f;
  float gs     = (float)LDTK_GRID_SIZE;

  float center_x = (float)gx * gs + gs / 2.0f - half_w;
  float center_y = half_h - ((float)gy * gs + gs / 2.0f);

  CF_V2 half_ext = cf_v2(gs / 2.0f, gs / 2.0f);
  return cf_make_aabb_center_half_extents(cf_v2(center_x, center_y), half_ext);
}

void sys_render_sprites(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) {
      continue;
    }
    if (!e->sprite.enabled) {
      continue;
    }
    if (!e->transform.enabled) {
      continue;
    }

    cf_draw_push();
    cf_draw_translate(e->transform.position.x, e->transform.position.y);
    cf_draw_sprite(&e->sprite.sprite);
    cf_draw_pop();
  }
}

void sys_debug_layers(void) {
  LdtkMap* map = &state->world.map;
  if (!map->loaded) {
    return;
  }

  LdtkLevel* level = &map->levels[map->active_level];
  int lw            = level->width;
  int lh            = level->height;
  float half_w      = (float)lw / 2.0f;
  float half_h      = (float)lh / 2.0f;
  float gs          = (float)LDTK_GRID_SIZE;

  // --- Grid lines (faint white) ---
  cf_draw_push_color(cf_make_color_rgba_f(1.0f, 1.0f, 1.0f, 0.12f));
  // Vertical lines
  for (int gx = 0; gx <= level->grid_width; gx++) {
    float x = (float)gx * gs - half_w;
    cf_draw_line(cf_v2(x, -half_h), cf_v2(x, half_h), 0.5f);
  }
  // Horizontal lines
  for (int gy = 0; gy <= level->grid_height; gy++) {
    float y = half_h - (float)gy * gs;
    cf_draw_line(cf_v2(-half_w, y), cf_v2(half_w, y), 0.5f);
  }
  cf_draw_pop_color();

  // --- IntGrid solid cells (yellow semi-transparent, full level) ---
  if (level->int_grid) {
    cf_draw_push_color(cf_make_color_rgba_f(1.0f, 1.0f, 0.0f, 0.2f));
    for (int gy = 0; gy < level->grid_height; gy++) {
      for (int gx = 0; gx < level->grid_width; gx++) {
        int val = ldtk_get_intgrid(map, map->active_level, gx, gy);
        if (val != 0) {
          CF_Aabb tile = debug_grid_cell_aabb(gx, gy, lw, lh);
          cf_draw_box_fill(tile, 0.0f);
        }
      }
    }
    cf_draw_pop_color();

    // IntGrid cell outlines (darker yellow wireframe)
    cf_draw_push_color(cf_make_color_rgba_f(1.0f, 0.8f, 0.0f, 0.4f));
    for (int gy = 0; gy < level->grid_height; gy++) {
      for (int gx = 0; gx < level->grid_width; gx++) {
        int val = ldtk_get_intgrid(map, map->active_level, gx, gy);
        if (val != 0) {
          CF_Aabb tile = debug_grid_cell_aabb(gx, gy, lw, lh);
          cf_draw_box(tile, 0.5f, 0.0f);
        }
      }
    }
    cf_draw_pop_color();
  }

  // --- Entity spawn points (cyan markers) ---
  for (int i = 0; i < level->entity_count; i++) {
    LdtkEntityInstance* ent = &level->entities[i];
    // Same conversion as ldtk_spawn_entities() in ldtk.c
    float cx = (float)ent->x - half_w;
    float cy = half_h - (float)ent->y;

    CF_V2 half_ext =
        cf_v2((float)ent->width / 2.0f, (float)ent->height / 2.0f);
    CF_Aabb box = cf_make_aabb_center_half_extents(cf_v2(cx, cy), half_ext);

    // Filled cyan box
    cf_draw_push_color(cf_make_color_rgba_f(0.0f, 1.0f, 1.0f, 0.15f));
    cf_draw_box_fill(box, 0.0f);
    cf_draw_pop_color();

    // Cyan wireframe
    cf_draw_push_color(cf_make_color_rgba_f(0.0f, 1.0f, 1.0f, 0.6f));
    cf_draw_box(box, 1.0f, 0.0f);
    cf_draw_pop_color();

    // Crosshair at center
    cf_draw_push_color(cf_make_color_rgba_f(0.0f, 1.0f, 1.0f, 0.5f));
    cf_draw_line(cf_v2(cx - 4.0f, cy), cf_v2(cx + 4.0f, cy), 0.5f);
    cf_draw_line(cf_v2(cx, cy - 4.0f), cf_v2(cx, cy + 4.0f), 0.5f);
    cf_draw_pop_color();

    // Entity identifier label
    cf_push_font_size(10.0f);
    cf_draw_push_color(cf_make_color_rgba_f(0.0f, 1.0f, 1.0f, 0.8f));
    cf_draw_text(ent->identifier, cf_v2(box.min.x, box.max.y + 2.0f), -1);
    cf_draw_pop_color();
    cf_pop_font_size();
  }

  // --- Layer labels (top-left of level, stacked) ---
  cf_push_font_size(10.0f);
  cf_draw_push_color(cf_make_color_rgba_f(1.0f, 1.0f, 1.0f, 0.7f));
  for (int i = 0; i < level->layer_count; i++) {
    LdtkLayerImage* layer = &level->layers[i];
    float label_y         = half_h - 2.0f - (float)i * 12.0f;
    cf_draw_text(layer->identifier, cf_v2(-half_w + 2.0f, label_y), -1);
  }
  cf_draw_pop_color();
  cf_pop_font_size();

  // --- Level bounds outline (magenta) ---
  CF_Aabb level_box = cf_make_aabb_center_half_extents(cf_v2(0, 0),
                                                        cf_v2(half_w, half_h));
  cf_draw_push_color(cf_make_color_rgba_f(1.0f, 0.0f, 1.0f, 0.6f));
  cf_draw_box(level_box, 1.0f, 0.0f);
  cf_draw_pop_color();
}

void sys_debug_colliders(void) {
  LdtkMap* map     = &state->world.map;
  LdtkLevel* level = map->loaded ? &map->levels[map->active_level] : nullptr;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists || !e->transform.enabled || !e->collider.enabled) {
      continue;
    }

    // Draw entity collider AABB (green if grounded, red if airborne)
    CF_V2 collider_center = cf_add(e->transform.position, e->collider.offset);
    CF_Aabb entity_aabb   = cf_make_aabb_center_half_extents(
        collider_center, e->collider.half_size);

    CF_Color col = e->collider.grounded
                       ? cf_make_color_rgba_f(0.0f, 1.0f, 0.0f, 0.4f)
                       : cf_make_color_rgba_f(1.0f, 0.0f, 0.0f, 0.4f);
    cf_draw_push_color(col);
    cf_draw_box_fill(entity_aabb, 0.0f);
    cf_draw_pop_color();

    // Draw nearby solid tiles (yellow, semi-transparent)
    if (level && level->int_grid) {
      int lw = level->width;
      int lh = level->height;

      int min_gx = debug_cf_x_to_grid(entity_aabb.min.x, lw);
      int max_gx = debug_cf_x_to_grid(entity_aabb.max.x, lw);
      int min_gy = debug_cf_y_to_grid(entity_aabb.max.y, lh);
      int max_gy = debug_cf_y_to_grid(entity_aabb.min.y, lh);

      if (min_gx < 0) {
        min_gx = 0;
      }
      if (min_gy < 0) {
        min_gy = 0;
      }
      if (max_gx >= level->grid_width) {
        max_gx = level->grid_width - 1;
      }
      if (max_gy >= level->grid_height) {
        max_gy = level->grid_height - 1;
      }

      cf_draw_push_color(cf_make_color_rgba_f(1.0f, 1.0f, 0.0f, 0.25f));
      for (int gy = min_gy; gy <= max_gy; gy++) {
        for (int gx = min_gx; gx <= max_gx; gx++) {
          if (ldtk_get_intgrid(map, map->active_level, gx, gy) != 0) {
            CF_Aabb tile = debug_grid_cell_aabb(gx, gy, lw, lh);
            cf_draw_box_fill(tile, 0.0f);
          }
        }
      }
      cf_draw_pop_color();
    }
  }
}
