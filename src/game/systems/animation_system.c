// animation_system.c - Coroutine-based player state and animation
//
// Single coroutine drives state transitions, animation selection, and sprite
// updates.

#include <cute_coroutine.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

// =============================================================================
// Coroutine Helpers
// =============================================================================

// Per-frame helper: update facing direction from input, apply sprite flip,
// update sprite
static void player_tick(CF_Coroutine co) {
  GameState* state           = (GameState*)cf_coroutine_get_udata(co);
  ecs_entity_t player_entity = state->world.player;

  auto controller = ECS_GET(player_entity, C_PlayerController);
  auto input      = ECS_GET(player_entity, C_PlayerInput);
  auto sprite     = ECS_GET(player_entity, C_Sprite);

  // Update facing direction from input
  if (input->right) {
    controller->facing_direction = cf_v2(1.0f, 0.0f);
  } else if (input->left) {
    controller->facing_direction = cf_v2(-1.0f, 0.0f);
  }

  // Apply sprite flip
  if (controller->facing_direction.x >= 0.0f) {
    sprite->scale.x = 1.0f;
  } else {
    sprite->scale.x = -1.0f;
  }

  // Update sprite animation
  cf_sprite_update(sprite);

  // Yield to next frame
  cf_coroutine_yield(co);
}

// =============================================================================
// Coroutine Entry Point
// =============================================================================

static void player_behavior_fn(CF_Coroutine co) {
  GameState* state           = (GameState*)cf_coroutine_get_udata(co);
  ecs_entity_t player_entity = state->world.player;

  while (true) {
    auto ps       = ECS_GET(player_entity, C_PlayerState);
    auto input    = ECS_GET(player_entity, C_PlayerInput);
    auto sprite   = ECS_GET(player_entity, C_Sprite);
    auto velocity = ECS_GET(player_entity, C_Velocity);

    // Priority-based branching: shoot+crouch > shoot > reload > crouch > walk >
    // idle

    // Shoot + Crouch → Crouch Fire (one-shot, return to crouching)
    if (input->shoot && input->crouch) {
      ps->current = PLAYER_STATE_CROUCH_FIRING;
      cf_sprite_play(sprite, "GunCrouchFire");

      // Loop until animation finishes
      while (!cf_sprite_will_finish(sprite)) {
        player_tick(co);
      }

      // Return to crouching state
      ps->current = PLAYER_STATE_CROUCHING;
      cf_sprite_play(sprite, "GunCrouch");
      player_tick(co);
      continue;
    }

    // Shoot → Fire (one-shot, pick GunWalkFire vs GunFire based on velocity)
    if (input->shoot) {
      ps->current = PLAYER_STATE_FIRING;

      // Pick animation based on current velocity
      bool moving           = velocity->x != 0.0f;
      const char* anim_name = moving ? "GunWalkFire" : "GunFire";
      cf_sprite_play(sprite, anim_name);

      // GunWalkFire has 8 frames but we only want 4 (one shot)
      if (moving) {
        while (cf_sprite_current_frame(sprite) < 3) {
          player_tick(co);
        }
      } else {
        // GunFire plays fully
        while (!cf_sprite_will_finish(sprite)) {
          player_tick(co);
        }
      }

      // Return to idle
      ps->current = PLAYER_STATE_IDLE;
      cf_sprite_play(sprite, "GunAim");
      player_tick(co);
      continue;
    }

    // Reload → Reload (one-shot, return to idle)
    if (input->reload) {
      ps->current = PLAYER_STATE_RELOADING;
      cf_sprite_play(sprite, "GunReload");

      // Loop until animation finishes
      while (!cf_sprite_will_finish(sprite)) {
        player_tick(co);
      }

      // Return to idle
      ps->current = PLAYER_STATE_IDLE;
      cf_sprite_play(sprite, "GunAim");
      player_tick(co);
      continue;
    }

    // Crouch → Crouching (looping)
    if (input->crouch) {
      ps->current = PLAYER_STATE_CROUCHING;
      if (!cf_sprite_is_playing(sprite, "GunCrouch")) {
        cf_sprite_play(sprite, "GunCrouch");
      }
      player_tick(co);
      continue;
    }

    // Walk → Walking (looping, requires horizontal velocity)
    if (velocity->x != 0.0f) {
      ps->current = PLAYER_STATE_WALKING;
      if (!cf_sprite_is_playing(sprite, "GunWalk")) {
        cf_sprite_play(sprite, "GunWalk");
      }
      player_tick(co);
      continue;
    }

    // Idle → Idle (looping)
    ps->current = PLAYER_STATE_IDLE;
    if (!cf_sprite_is_playing(sprite, "GunAim")) {
      cf_sprite_play(sprite, "GunAim");
    }
    player_tick(co);
  }
}

// =============================================================================
// System: Player Coroutine
// =============================================================================
// Resumes player coroutine each frame, creates it if dead/uninitialized

// NOLINTBEGIN
ecs_ret_t sys_player_coroutine([[maybe_unused]] ecs_t* ecs,
                               ecs_entity_t* entities, size_t count,
                               [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto ps = ECS_GET(entities[i], C_PlayerState);

    // Create coroutine if uninitialized or dead
    if (ps->co.id == 0 ||
        cf_coroutine_state(ps->co) == CF_COROUTINE_STATE_DEAD) {
      ps->co = cf_make_coroutine(player_behavior_fn, 0, state);
    }

    // Resume coroutine
    cf_coroutine_resume(ps->co);
  }

  return 0;
}
// NOLINTEND
