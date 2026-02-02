// input_system.c - Input gathering system
//
// Reads keyboard state and populates the input component.
// Movement uses held state, actions use single-frame triggers.

#include <cute_input.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

ecs_ret_t sys_gather_input([[maybe_unused]] ecs_t* ecs, ecs_entity_t* entities,
                           size_t count, [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; ++i) {
    auto input = ECS_GET(entities[i], C_PlayerInput);

    // Movement directions (held state)
    input->up    = cf_key_down(CF_KEY_W) || cf_key_down(CF_KEY_UP);
    input->down  = cf_key_down(CF_KEY_S) || cf_key_down(CF_KEY_DOWN);
    input->left  = cf_key_down(CF_KEY_A) || cf_key_down(CF_KEY_LEFT);
    input->right = cf_key_down(CF_KEY_D) || cf_key_down(CF_KEY_RIGHT);

    // Movement modifiers (held state)
    input->crouch = cf_key_down(CF_KEY_LCTRL);

    // Action triggers (single-frame)
    input->shoot  = cf_mouse_just_pressed(CF_MOUSE_BUTTON_LEFT);
    input->reload = cf_key_just_pressed(CF_KEY_R);
  }

  return 0;
}
