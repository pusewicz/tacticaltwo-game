// player_system.c - Player state and movement systems
//
// sys_update_player_state: State machine (idle, walking, crouching, firing,
// etc.) sys_update_player_movement: Sets velocity based on state and input

#include <cute_math.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

// =============================================================================
// System: Update Player State
// =============================================================================
// Determines player state based on input.
// Priority: Aiming > Crouch Walking > Crouching > Walking > Idle

ecs_ret_t sys_update_player_state([[maybe_unused]] ecs_t* ecs,
                                  ecs_entity_t* entities, size_t count,
                                  [[maybe_unused]] void* udata) {
  float dt = state->world.dt;

  for (size_t i = 0; i < count; ++i) {
    auto ps    = ECS_GET(entities[i], C_PlayerState);
    auto input = ECS_GET(entities[i], C_PlayerInput);

    // Check if player is moving horizontally
    bool moving = input->left || input->right;

    // Store previous state for transition detection
    ps->previous = ps->current;

    // Lock state during reloading or firing - wait for animation to finish
    if (ps->current == PLAYER_STATE_RELOADING ||
        ps->current == PLAYER_STATE_FIRING ||
        ps->current == PLAYER_STATE_CROUCH_FIRING) {
      ps->state_timer += dt;
      continue;
    }

    // Determine new state based on input priority
    // Firing state triggered by shoot input (crouch firing if crouching)
    if (input->shoot && input->crouch) {
      ps->current = PLAYER_STATE_CROUCH_FIRING;
    } else if (input->shoot) {
      ps->current = PLAYER_STATE_FIRING;
    } else if (input->reload) {
      ps->current = PLAYER_STATE_RELOADING;
    } else if (input->crouch) {
      ps->current = PLAYER_STATE_CROUCHING;
    } else if (moving) {
      ps->current = PLAYER_STATE_WALKING;
    } else {
      ps->current = PLAYER_STATE_IDLE;
    }

    // Update state timer
    if (ps->current != ps->previous) {
      ps->state_timer = 0.0f;
    } else {
      ps->state_timer += dt;
    }
  }

  return 0;
}

// =============================================================================
// System: Update Player Movement
// =============================================================================
// Sets velocity based on state and input.
// Speed varies by state: walk > crouch > aim

ecs_ret_t sys_update_player_movement([[maybe_unused]] ecs_t* ecs,
                                     ecs_entity_t* entities, size_t count,
                                     [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto velocity   = ECS_GET(entities[i], C_Velocity);
    auto controller = ECS_GET(entities[i], C_PlayerController);
    auto ps         = ECS_GET(entities[i], C_PlayerState);
    auto input      = ECS_GET(entities[i], C_PlayerInput);

    // No movement while crouching
    if (ps->current == PLAYER_STATE_CROUCHING ||
        ps->current == PLAYER_STATE_CROUCH_FIRING) {
      velocity->x = 0.0f;
      velocity->y = 0.0f;
    } else {
      // Calculate horizontal velocity
      velocity->x = 0.0f;
      if (input->left) {
        velocity->x -= controller->walk_speed;
      }
      if (input->right) {
        velocity->x += controller->walk_speed;
      }

      // No vertical movement for side-scroller (no jumping initially)
      velocity->y = 0.0f;
    }

    // Update facing direction based on input
    if (input->right && !input->left) {
      controller->facing_direction = cf_v2(1.0f, 0.0f);
    } else if (input->left && !input->right) {
      controller->facing_direction = cf_v2(-1.0f, 0.0f);
    }
  }

  return 0;
}
