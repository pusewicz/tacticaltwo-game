// world.c - ECS World Management
//
// Initializes ECS, registers components and systems, manages world lifecycle.
// System implementations are in the systems/ directory.

#define PICO_ECS_IMPLEMENTATION

#include "world.h"

#include <cute_coroutine.h>
#include <cute_map.h>
#include <cute_math.h>
#include <cute_sprite.h>

#include "../engine/game_state.h"
#include "systems/systems.h"

// =============================================================================
// Player Factory
// =============================================================================

void make_player(void) {
  // Create player entity and store in world
  ecs_entity_t player = ecs_create(state->world.ecs);
  state->world.player = player;

  // Add components to player
  ECS_ADD(player, C_PlayerInput);

  // Initialize player controller with default speeds
  auto controller              = ECS_ADD(player, C_PlayerController);
  controller->walk_speed       = 150.0f;
  controller->facing_direction = cf_v2(1.0f, 0.0f); // Default: facing right

  // Initialize player state
  auto ps     = ECS_ADD(player, C_PlayerState);
  ps->current = PLAYER_STATE_IDLE;
  ps->co.id   = 0; // Coroutine created on first tick

  // Initialize transform at center of screen (CF origin is at center)
  auto transform      = ECS_ADD(player, C_Transform);
  transform->position = cf_v2(0.0f, 0.0f);
  transform->rotation = 0.0f;

  // Initialize velocity (stationary)
  auto velocity = ECS_ADD(player, C_Velocity);
  *velocity     = cf_v2(0.0f, 0.0f);

  // Initialize sprite with player_combat.ase (gun animations)
  auto sprite = ECS_ADD(player, C_Sprite);
  *sprite     = cf_make_sprite("assets/sprites/player_combat.ase");

  // Start with walk animation
  cf_sprite_play(sprite, "GunWalk");
}

// =============================================================================
// World Initialization
// =============================================================================

// NOLINTBEGIN
void init_world(void) {
  // Create ECS context
  state->world.ecs        = ecs_new(ECS_ENTITY_COUNT, nullptr);
  state->world.components = nullptr;
  state->world.systems    = nullptr;
  state->world.dt         = 0.0f;

  // Register components
  ECS_REGISTER_COMP(C_PlayerInput);
  ECS_REGISTER_COMP(C_PlayerController);
  ECS_REGISTER_COMP(C_PlayerState);
  ECS_REGISTER_COMP(C_Transform);
  ECS_REGISTER_COMP(C_Velocity);
  ECS_REGISTER_COMP(C_Sprite);

  // Register systems
  ECS_REGISTER_SYSTEM(sys_gather_input, nullptr);
  ECS_REQUIRE_COMP(sys_gather_input, C_PlayerInput);

  ECS_REGISTER_SYSTEM(sys_player_coroutine, nullptr);
  ECS_REQUIRE_COMP(sys_player_coroutine, C_PlayerState);

  ECS_REGISTER_SYSTEM(sys_update_player_movement, nullptr);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_Velocity);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerController);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerState);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerInput);

  ECS_REGISTER_SYSTEM(sys_apply_velocity, nullptr);
  ECS_REQUIRE_COMP(sys_apply_velocity, C_Transform);
  ECS_REQUIRE_COMP(sys_apply_velocity, C_Velocity);

  ECS_REGISTER_SYSTEM(sys_render_sprites, nullptr);
  ECS_REQUIRE_COMP(sys_render_sprites, C_Sprite);
  ECS_REQUIRE_COMP(sys_render_sprites, C_Transform);

  // Create player using factory function
  make_player();
}
// NOLINTEND

// =============================================================================
// Main Update Function
// =============================================================================
// Runs systems in explicit order for control over execution pipeline.

void update_world(float dt) {
  state->world.dt = dt;

  // Input
  ECS_RUN_SYSTEM(sys_gather_input);

  // Logic - coroutine drives state and animation
  ECS_RUN_SYSTEM(sys_player_coroutine);
  ECS_RUN_SYSTEM(sys_update_player_movement);

  // Physics
  ECS_RUN_SYSTEM(sys_apply_velocity);
}

// =============================================================================
// Render World
// =============================================================================
// Renders all sprites using the render system.

void render_world(void) { ECS_RUN_SYSTEM(sys_render_sprites); }

// =============================================================================
// Hot Reload
// =============================================================================
// Updates system callbacks after the game library is reloaded.
// Function pointers become stale when the library is unloaded/reloaded.

void world_hot_reload(void) {
  // Destroy stale coroutine (saved context points to old code segment)
  auto ps = ECS_GET(state->world.player, C_PlayerState);
  if (ps->co.id != 0) {
    cf_destroy_coroutine(ps->co);
    ps->co.id = 0; // Will be recreated on next tick
  }

  // Update system callbacks
  ECS_UPDATE_SYSTEM(sys_gather_input, nullptr);
  ECS_UPDATE_SYSTEM(sys_player_coroutine, nullptr);
  ECS_UPDATE_SYSTEM(sys_update_player_movement, nullptr);
  ECS_UPDATE_SYSTEM(sys_apply_velocity, nullptr);
  ECS_UPDATE_SYSTEM(sys_render_sprites, nullptr);
}

// =============================================================================
// Shutdown
// =============================================================================

void shutdown_world(void) {
  // Destroy coroutine before freeing ECS
  auto ps = ECS_GET(state->world.player, C_PlayerState);
  if (ps->co.id != 0) {
    cf_destroy_coroutine(ps->co);
  }

  if (state->world.ecs) {
    ecs_free(state->world.ecs);
    state->world.ecs = nullptr;
  }
  if (state->world.components) {
    cf_map_free(state->world.components);
  }
  if (state->world.systems) {
    cf_map_free(state->world.systems);
  }
}
