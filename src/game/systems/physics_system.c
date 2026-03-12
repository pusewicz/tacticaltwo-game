// physics_system.c - Physics integration and collision systems
//
// Euler integration + AABB tilemap collision resolution.

#include <cute_math.h>
#include <math.h>

#include "../../engine/game_state.h"
#include "../ldtk.h"
#include "../world.h"
#include "systems.h"

void sys_apply_velocity(void) {
  float dt = state->world.dt;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) {
      continue;
    }
    if (!e->transform.enabled) {
      continue;
    }
    if (!e->velocity.enabled) {
      continue;
    }

    e->transform.position =
        cf_add(e->transform.position, cf_mul(e->velocity.value, dt));
  }
}

// =============================================================================
// Coordinate helpers: CF world coords <-> LDtk grid coords
// =============================================================================

// CF world X -> LDtk grid column
static int cf_x_to_grid(float cf_x, int level_width) {
  float ldtk_x = cf_x + (float)level_width / 2.0f;
  return (int)floorf(ldtk_x / (float)LDTK_GRID_SIZE);
}

// CF world Y -> LDtk grid row
static int cf_y_to_grid(float cf_y, int level_height) {
  float ldtk_y = (float)level_height / 2.0f - cf_y;
  return (int)floorf(ldtk_y / (float)LDTK_GRID_SIZE);
}

// LDtk grid cell -> CF world AABB
static CF_Aabb grid_cell_aabb(int gx, int gy, int level_width,
                              int level_height) {
  float half_w = (float)level_width / 2.0f;
  float half_h = (float)level_height / 2.0f;
  float gs     = (float)LDTK_GRID_SIZE;

  float center_x = (float)gx * gs + gs / 2.0f - half_w;
  float center_y = half_h - ((float)gy * gs + gs / 2.0f);

  CF_V2 half_ext = cf_v2(gs / 2.0f, gs / 2.0f);
  return cf_make_aabb_center_half_extents(cf_v2(center_x, center_y), half_ext);
}

// =============================================================================
// Tilemap collision system
// =============================================================================

void sys_collide_tilemap(void) {
  LdtkMap* map = &state->world.map;
  if (!map->loaded) {
    return;
  }

  int level_idx    = map->active_level;
  LdtkLevel* level = &map->levels[level_idx];
  if (!level->int_grid) {
    return;
  }

  int lw = level->width;
  int lh = level->height;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists || !e->transform.enabled || !e->velocity.enabled ||
        !e->collider.enabled) {
      continue;
    }

    e->collider.grounded = false;

    // Build entity AABB (offset from transform position)
    CF_V2 collider_center = cf_add(e->transform.position, e->collider.offset);
    CF_Aabb entity_aabb =
        cf_make_aabb_center_half_extents(collider_center, e->collider.half_size);

    // Compute grid cell range that overlaps the entity AABB (with 1-cell
    // margin)
    int min_gx = cf_x_to_grid(entity_aabb.min.x, lw) - 1;
    int max_gx = cf_x_to_grid(entity_aabb.max.x, lw) + 1;
    int min_gy = cf_y_to_grid(entity_aabb.max.y, lh) - 1; // max.y = top in CF
    int max_gy = cf_y_to_grid(entity_aabb.min.y, lh) + 1; // min.y = bottom

    // Clamp to grid bounds
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

    for (int gy = min_gy; gy <= max_gy; gy++) {
      for (int gx = min_gx; gx <= max_gx; gx++) {
        if (ldtk_get_intgrid(map, level_idx, gx, gy) == 0) {
          continue;
        }

        CF_Aabb tile_aabb = grid_cell_aabb(gx, gy, lw, lh);
        CF_Manifold m     = cf_aabb_to_aabb_manifold(entity_aabb, tile_aabb);
        if (m.count == 0) {
          continue;
        }

        // Push entity out: n points A->B (entity->tile), so push by -n*depth
        float depth           = m.depths[0];
        CF_V2 push            = cf_mul(m.n, -depth);
        e->transform.position = cf_add(e->transform.position, push);

        // Rebuild entity AABB after push
        collider_center = cf_add(e->transform.position, e->collider.offset);
        entity_aabb =
            cf_make_aabb_center_half_extents(collider_center, e->collider.half_size);

        // Resolve velocity based on collision axis
        float nx = m.n.x;
        float ny = m.n.y;

        if (fabsf(nx) > fabsf(ny)) {
          // Horizontal collision
          e->velocity.value.x = 0.0f;
        } else {
          // Vertical collision
          if (ny < -0.5f) {
            // Floor (n points down = entity is above tile)
            e->collider.grounded = true;
            if (e->velocity.value.y < 0.0f) {
              e->velocity.value.y = 0.0f;
            }
          } else if (ny > 0.5f) {
            // Ceiling (n points up = entity is below tile)
            if (e->velocity.value.y > 0.0f) {
              e->velocity.value.y = 0.0f;
            }
          }
        }
      }
    }
  }
}
