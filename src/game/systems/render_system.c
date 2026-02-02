// render_system.c - Sprite rendering system
//
// Draws all entities with Sprite and Transform components.

#include <cute_draw.h>
#include <stddef.h>

#include "../../engine/game_state.h"
#include "systems.h"
#include "world.h"

ecs_ret_t sys_render_sprites([[maybe_unused]] ecs_t* ecs,
                             ecs_entity_t* entities, size_t count,
                             [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto sprite    = ECS_GET(entities[i], C_Sprite);
    auto transform = ECS_GET(entities[i], C_Transform);

    cf_draw_push();
    cf_draw_translate(transform->position.x, transform->position.y);
    cf_draw_sprite(sprite);
    cf_draw_pop();
  }
  return 0;
}
