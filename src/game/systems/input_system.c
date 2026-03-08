// input_system.c - Input gathering system
//
// Reads keyboard state and populates the PlayerInput component.

#include <cute_input.h>

#include "../../engine/game_state.h"
#include "../world.h"

void sys_gather_input(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) continue;
    if (!e->player_input.enabled) continue;

    e->player_input.up    = cf_key_down(CF_KEY_W) || cf_key_down(CF_KEY_UP);
    e->player_input.down  = cf_key_down(CF_KEY_S) || cf_key_down(CF_KEY_DOWN);
    e->player_input.left  = cf_key_down(CF_KEY_A) || cf_key_down(CF_KEY_LEFT);
    e->player_input.right = cf_key_down(CF_KEY_D) || cf_key_down(CF_KEY_RIGHT);

    e->player_input.crouch = cf_key_down(CF_KEY_LCTRL);

    e->player_input.shoot  = cf_mouse_just_pressed(CF_MOUSE_BUTTON_LEFT);
    e->player_input.reload = cf_key_just_pressed(CF_KEY_R);
  }
}
