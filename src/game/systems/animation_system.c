// animation_system.c - Coroutine-based player state and animation
//
// Single coroutine drives state transitions, animation selection, and sprite
// updates.

#include <cute_coroutine.h>
#include <cute_math.h>
#include <cute_sprite.h>

#include "../../engine/game_state.h"
#include "../world.h"

// =============================================================================
// Coroutine Helpers
// =============================================================================

static void player_tick(CF_Coroutine co) {
  GameState* gs = (GameState*)cf_coroutine_get_udata(co);
  Entity* e     = &gs->world.entities[gs->world.player];

  if (e->player_input.right) {
    e->player_controller.facing_direction = cf_v2(1.0f, 0.0f);
  } else if (e->player_input.left) {
    e->player_controller.facing_direction = cf_v2(-1.0f, 0.0f);
  }

  if (e->player_controller.facing_direction.x >= 0.0f) {
    e->sprite.sprite.scale.x = 1.0f;
  } else {
    e->sprite.sprite.scale.x = -1.0f;
  }

  cf_sprite_update(&e->sprite.sprite);
  cf_coroutine_yield(co);
}

// =============================================================================
// Coroutine Entry Point
// =============================================================================

static void player_behavior_fn(CF_Coroutine co) {
  GameState* gs = (GameState*)cf_coroutine_get_udata(co);

  while (true) {
    Entity* e = &gs->world.entities[gs->world.player];

    PlayerStateComp* ps = &e->player_state;
    PlayerInput* input  = &e->player_input;
    CF_Sprite* sprite   = &e->sprite.sprite;
    Velocity* velocity  = &e->velocity;

    if (input->shoot && input->crouch) {
      ps->current = PLAYER_STATE_CROUCH_FIRING;
      cf_sprite_play(sprite, "GunCrouchFire");
      while (!cf_sprite_will_finish(sprite)) {
        player_tick(co);
      }
      ps->current = PLAYER_STATE_CROUCHING;
      cf_sprite_play(sprite, "GunCrouch");
      player_tick(co);
      continue;
    }

    if (input->shoot) {
      ps->current           = PLAYER_STATE_FIRING;
      bool moving           = velocity->value.x != 0.0f;
      const char* anim_name = moving ? "GunWalkFire" : "GunFire";
      cf_sprite_play(sprite, anim_name);
      if (moving) {
        while (cf_sprite_current_frame(sprite) < 3) {
          player_tick(co);
        }
      } else {
        while (!cf_sprite_will_finish(sprite)) {
          player_tick(co);
        }
      }
      ps->current = PLAYER_STATE_IDLE;
      cf_sprite_play(sprite, "GunAim");
      player_tick(co);
      continue;
    }

    if (input->reload) {
      ps->current = PLAYER_STATE_RELOADING;
      cf_sprite_play(sprite, "GunReload");
      while (!cf_sprite_will_finish(sprite)) {
        player_tick(co);
      }
      ps->current = PLAYER_STATE_IDLE;
      cf_sprite_play(sprite, "GunAim");
      player_tick(co);
      continue;
    }

    if (input->crouch) {
      ps->current = PLAYER_STATE_CROUCHING;
      if (!cf_sprite_is_playing(sprite, "GunCrouch")) {
        cf_sprite_play(sprite, "GunCrouch");
      }
      player_tick(co);
      continue;
    }

    if (velocity->value.x != 0.0f) {
      ps->current = PLAYER_STATE_WALKING;
      if (!cf_sprite_is_playing(sprite, "GunWalk")) {
        cf_sprite_play(sprite, "GunWalk");
      }
      player_tick(co);
      continue;
    }

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

void sys_player_coroutine(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) continue;
    if (!e->player_state.enabled) continue;

    if (e->player_state.co.id == 0 ||
        cf_coroutine_state(e->player_state.co) == CF_COROUTINE_STATE_DEAD) {
      e->player_state.co = cf_make_coroutine(player_behavior_fn, 0, state);
    }

    cf_coroutine_resume(e->player_state.co);
  }
}
