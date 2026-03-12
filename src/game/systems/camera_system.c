// camera_system.c - Smooth follow camera with level bounds clamping

#include <cute_math.h>

#include "../../config/config.h"
#include "../../engine/game_state.h"
#include "../ldtk.h"
#include "../world.h"
#include "systems.h"

#define CAMERA_LERP_SPEED 5.0f

void sys_camera_follow(void) {
  int p = state->world.player;
  if (p == ENTITY_NONE) {
    return;
  }

  Entity* player = &state->world.entities[p];
  if (!player->exists || !player->transform.enabled) {
    return;
  }

  CF_V2 target = player->transform.position;
  float dt     = state->world.dt;

  // Lerp toward target
  CF_V2 diff       = cf_sub(target, state->world.camera);
  state->world.camera = cf_add(state->world.camera, cf_mul(diff, CAMERA_LERP_SPEED * dt));

  // Clamp to level bounds
  LdtkMap* map = &state->world.map;
  if (!map->loaded) {
    return;
  }

  LdtkLevel* level = &map->levels[map->active_level];
  float half_level_w  = (float)level->width / 2.0f;
  float half_level_h  = (float)level->height / 2.0f;
  float half_canvas_w = (float)CANVAS_WIDTH / 2.0f;
  float half_canvas_h = (float)CANVAS_HEIGHT / 2.0f;

  // If level is smaller than canvas on an axis, center it
  if ((float)level->width <= (float)CANVAS_WIDTH) {
    state->world.camera.x = 0.0f;
  } else {
    float min_x = -half_level_w + half_canvas_w;
    float max_x = half_level_w - half_canvas_w;
    if (state->world.camera.x < min_x) {
      state->world.camera.x = min_x;
    }
    if (state->world.camera.x > max_x) {
      state->world.camera.x = max_x;
    }
  }

  if ((float)level->height <= (float)CANVAS_HEIGHT) {
    state->world.camera.y = 0.0f;
  } else {
    float min_y = -half_level_h + half_canvas_h;
    float max_y = half_level_h - half_canvas_h;
    if (state->world.camera.y < min_y) {
      state->world.camera.y = min_y;
    }
    if (state->world.camera.y > max_y) {
      state->world.camera.y = max_y;
    }
  }
}
