// physics_system.c - Physics integration system
//
// Integrates velocity into position using simple Euler integration.

#include <cute_math.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

ecs_ret_t sys_apply_velocity([[maybe_unused]] ecs_t* ecs,
                             ecs_entity_t* entities, size_t count,
                             [[maybe_unused]] void* udata) {
  float dt = state->world.dt;

  for (size_t i = 0; i < count; i++) {
    auto transform = ECS_GET(entities[i], C_Transform);
    auto velocity  = ECS_GET(entities[i], C_Velocity);

    // Simple Euler integration: position += velocity * dt
    transform->position = cf_add(transform->position, cf_mul(*velocity, dt));
  }

  return 0;
}
