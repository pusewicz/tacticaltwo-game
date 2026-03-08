// player_system.c - Player movement system
//
// Sets velocity based on state and input.

#include <cute_math.h>

#include "../../engine/game_state.h"
#include "../world.h"

void sys_update_player_movement(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) continue;
    if (!e->velocity.enabled) continue;
    if (!e->player_controller.enabled) continue;
    if (!e->player_state.enabled) continue;
    if (!e->player_input.enabled) continue;

    if (e->player_state.current == PLAYER_STATE_CROUCHING ||
        e->player_state.current == PLAYER_STATE_CROUCH_FIRING) {
      e->velocity.value.x = 0.0f;
      e->velocity.value.y = 0.0f;
    } else {
      e->velocity.value.x = 0.0f;
      if (e->player_input.left) {
        e->velocity.value.x -= e->player_controller.walk_speed;
      }
      if (e->player_input.right) {
        e->velocity.value.x += e->player_controller.walk_speed;
      }
      e->velocity.value.y = 0.0f;
    }
  }
}
