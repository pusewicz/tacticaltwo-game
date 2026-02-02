// animation_system.c - Animation management system
//
// Maps player state to animation and updates sprite.

#include <cute_sprite.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

// =============================================================================
// Animation Mapping
// =============================================================================

static const char* state_to_animation(PlayerState ps) {
  switch (ps) {
  case PLAYER_STATE_IDLE:
    return "GunAim";
  case PLAYER_STATE_WALKING:
    return "GunWalk";
  case PLAYER_STATE_CROUCHING:
  case PLAYER_STATE_CROUCH_WALKING:
    return "GunCrouch";
  case PLAYER_STATE_FIRING:
    return "GunFire";
  case PLAYER_STATE_CROUCH_FIRING:
    return "GunCrouchFire";
  case PLAYER_STATE_RELOADING:
    return "GunReload";
  default:
    return "GunAim";
  }
}

// =============================================================================
// System: Update Animation
// =============================================================================
// Maps player state to animation and updates sprite.

// NOLINTBEGIN
ecs_ret_t sys_update_animation([[maybe_unused]] ecs_t* ecs,
                               ecs_entity_t* entities, size_t count,
                               [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto sprite     = ECS_GET(entities[i], C_Sprite);
    auto ps         = ECS_GET(entities[i], C_PlayerState);
    auto controller = ECS_GET(entities[i], C_PlayerController);
    auto velocity   = ECS_GET(entities[i], C_Velocity);

    // Get animation name for current state
    const char* anim_name = state_to_animation(ps->current);

    // Use walk+fire animation if firing while moving
    // Only pick fire animation at start of firing (don't switch mid-animation)
    if (ps->current == PLAYER_STATE_FIRING) {
      if (cf_sprite_is_playing(sprite, "GunFire") ||
          cf_sprite_is_playing(sprite, "GunWalkFire")) {
        // Already in firing animation - don't change it
        anim_name = nullptr;
      } else {
        // Just started firing - pick animation based on current velocity
        bool moving = velocity->x != 0.0f;
        anim_name   = moving ? "GunWalkFire" : "GunFire";
      }
    }

    // Crouch fire animation - no movement variant needed
    if (ps->current == PLAYER_STATE_CROUCH_FIRING) {
      if (cf_sprite_is_playing(sprite, "GunCrouchFire")) {
        anim_name = nullptr;
      } else {
        anim_name = "GunCrouchFire";
      }
    }

    // Only call cf_sprite_play when animation changes
    if (anim_name && !cf_sprite_is_playing(sprite, anim_name)) {
      cf_sprite_play(sprite, anim_name);
    }

    // Update sprite animation every frame
    cf_sprite_update(sprite);

    // Check if reloading or firing animation should finish
    // Only check after at least one frame (state_timer > 0)
    if (ps->state_timer > 0.0f && (ps->current == PLAYER_STATE_RELOADING ||
                                   ps->current == PLAYER_STATE_FIRING ||
                                   ps->current == PLAYER_STATE_CROUCH_FIRING)) {
      bool should_finish = false;

      // GunWalkFire has 8 frames but we only want 4 (one shot)
      if (cf_sprite_is_playing(sprite, "GunWalkFire")) {
        should_finish = cf_sprite_current_frame(sprite) >= 3;
      } else {
        should_finish = cf_sprite_will_finish(sprite);
      }

      if (should_finish) {
        // Return to crouching if was crouch firing, otherwise idle
        if (ps->current == PLAYER_STATE_CROUCH_FIRING) {
          ps->current = PLAYER_STATE_CROUCHING;
        } else {
          ps->current = PLAYER_STATE_IDLE;
        }
      }
    }

    // Set horizontal flip based on facing direction
    if (controller->facing_direction.x >= 0.0f) {
      sprite->scale.x = 1.0f;
    } else {
      sprite->scale.x = -1.0f;
    }
  }

  return 0;
}
// NOLINTEND
