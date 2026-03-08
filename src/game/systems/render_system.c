// render_system.c - Sprite rendering system
//
// Draws all entities with Sprite and Transform components.

#include <cute_draw.h>

#include "../../engine/game_state.h"
#include "../world.h"
#include "systems.h"

void sys_render_sprites(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists) {
      continue;
    }
    if (!e->sprite.enabled) {
      continue;
    }
    if (!e->transform.enabled) {
      continue;
    }

    cf_draw_push();
    cf_draw_translate(e->transform.position.x, e->transform.position.y);
    cf_draw_sprite(&e->sprite.sprite);
    cf_draw_pop();
  }
}
