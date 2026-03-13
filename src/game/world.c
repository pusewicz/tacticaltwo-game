// world.c - World management with fat struct entities
//
// Entity lifecycle, player factory, and system dispatch.

#include "world.h"

#include <cute_coroutine.h>
#include <cute_math.h>
#include <cute_result.h>
#include <cute_sprite.h>
#include <string.h>

#include "../engine/game_state.h"
#include "../engine/log.h"
#include "ldtk.h"
#include "rain.h"
#include "systems/systems.h"

// =============================================================================
// Entity Management
// =============================================================================

int world_add_entity(Entity e) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (!state->world.entities[i].exists) {
      state->world.entities[i] = e;
      return i;
    }
  }
  return ENTITY_NONE;
}

void world_remove_entity(int id) {
  if (id >= 0 && id < MAX_ENTITIES) {
    memset(&state->world.entities[id], 0, sizeof(Entity));
  }
}

// =============================================================================
// Player Factory
// =============================================================================

void make_player_at(float x, float y) {
  CF_Sprite player_sprite = cf_make_sprite("assets/sprites/player_combat.ase");
  cf_sprite_play(&player_sprite, "GunWalk");

  int id = world_add_entity((Entity){
      .exists       = true,
      .player_input = {.enabled = true},
      .player_controller =
          {
              .enabled          = true,
              .walk_speed       = 150.0f,
              .facing_direction = cf_v2(1.0f, 0.0f),
          },
      .player_state =
          {
              .enabled = true,
              .current = PLAYER_STATE_IDLE,
          },
      .transform =
          {
              .enabled  = true,
              .position = cf_v2(x, y),
          },
      .velocity = {.enabled = true},
      .collider =
          {
              .enabled   = true,
              .half_size = cf_v2(7.0f, 14.0f),
              .offset    = cf_v2(0.0f, -28.0f),
              .grounded  = false,
          },
      .sprite =
          {
              .enabled = true,
              .sprite  = player_sprite,
          },
  });

  state->world.player = id;
}

// =============================================================================
// World Lifecycle
// =============================================================================

void init_world(void) {
  memset(&state->world, 0, sizeof(World));
  state->world.player = ENTITY_NONE;

  if (ldtk_load(&state->world.map, "/assets/ldtk/map/simplified")) {
    ldtk_spawn_entities(&state->world.map, 0);
  } else {
    log_warn("world", "LDtk load failed, spawning player at default position");
    make_player_at(0.0f, 0.0f);
  }

  // Load parallax backgrounds
  {
    Parallax* px    = &state->world.parallax;
    CF_Result result = {0};

    px->sky = cf_make_easy_sprite_from_png(
        "/assets/GandalfHardcore City Tiles/City background sky.png", &result);
    if (cf_is_error(result)) {
      log_warn("world", "Failed to load parallax sky");
    }

    px->layer1 = cf_make_easy_sprite_from_png(
        "/assets/GandalfHardcore City Tiles/City background layer1.png",
        &result);
    if (cf_is_error(result)) {
      log_warn("world", "Failed to load parallax layer1");
    }

    px->layer2 = cf_make_easy_sprite_from_png(
        "/assets/GandalfHardcore City Tiles/City background layer2.png",
        &result);
    if (cf_is_error(result)) {
      log_warn("world", "Failed to load parallax layer2");
    }

    px->loaded = !cf_is_error(result);
  }

  // Snap camera to player spawn (no lerp on first frame)
  int p = state->world.player;
  if (p != ENTITY_NONE) {
    state->world.camera = state->world.entities[p].transform.position;
  }

  rain_init();
}

void update_world(float dt) {
  // Clamp dt to prevent physics explosion on first frame or lag spikes
  if (dt > 1.0f / 20.0f) {
    dt = 1.0f / 20.0f;
  }
  state->world.dt = dt;

  if (ldtk_check_reload(&state->world.map)) {
    rain_rebuild_height_map();
  }

  sys_gather_input();
  sys_player_coroutine();
  sys_update_player_movement();
  sys_apply_velocity();
  sys_collide_tilemap();
  sys_camera_follow();
}

void render_world(void) {
  sys_render_parallax();

  cf_draw_push();
  cf_draw_translate(-state->world.camera.x, -state->world.camera.y);

  sys_render_tilemap();
  sys_render_sprites();

  if (state->debug_mode) {
    sys_debug_layers();
    sys_debug_colliders();
  }

  cf_draw_pop();

  rain_render(state->world.dt);
}

void world_hot_reload(void) {
  int p = state->world.player;
  if (p == ENTITY_NONE) {
    return;
  }

  Entity* player = &state->world.entities[p];
  if (player->player_state.enabled && player->player_state.co.id != 0) {
    cf_destroy_coroutine(player->player_state.co);
    player->player_state.co.id = 0;
  }
}

void shutdown_world(void) {
  rain_shutdown();

  // Destroy any active coroutines
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (e->exists && e->player_state.enabled && e->player_state.co.id != 0) {
      cf_destroy_coroutine(e->player_state.co);
    }
  }

  // Unload parallax sprites
  if (state->world.parallax.loaded) {
    cf_easy_sprite_unload(&state->world.parallax.sky);
    cf_easy_sprite_unload(&state->world.parallax.layer1);
    cf_easy_sprite_unload(&state->world.parallax.layer2);
  }

  ldtk_unload(&state->world.map);
}
