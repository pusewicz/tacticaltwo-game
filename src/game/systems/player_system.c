// player_system.c - Player movement system
//
// sys_update_player_movement: Sets velocity based on state and input

#include <cute_math.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

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
  }

  return 0;
}
