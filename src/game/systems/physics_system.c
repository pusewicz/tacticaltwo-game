// physics_system.c - Physics integration system
//
// Integrates velocity into position using simple Euler integration.

#include <cute_math.h>

#include "../../engine/game_state.h"
#include "../world.h"

void sys_apply_velocity(void) {
  float dt = state->world.dt;

  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) continue;
    if (!e->transform.enabled) continue;
    if (!e->velocity.enabled) continue;

    e->transform.position =
        cf_add(e->transform.position, cf_mul(e->velocity.value, dt));
  }
}
