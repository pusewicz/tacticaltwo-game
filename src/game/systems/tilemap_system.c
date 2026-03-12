// tilemap_system.c - Renders LDtk tilemap layers
//
// Draws pre-rendered layer PNGs for the active level.

#include <cute_draw.h>

#include "../../engine/game_state.h"
#include "../ldtk.h"
#include "../world.h"
#include "systems.h"

void sys_render_tilemap(void) {
  LdtkMap* map = &state->world.map;
  if (!map->loaded) {
    return;
  }

  if (map->active_level < 0 || map->active_level >= map->level_count) {
    return;
  }

  LdtkLevel* level = &map->levels[map->active_level];

  // Render layers bottom-to-top (LDtk lists top-to-bottom)
  for (int i = level->layer_count - 1; i >= 0; i--) {
    LdtkLayerImage* layer = &level->layers[i];
    if (!layer->loaded) {
      continue;
    }
    cf_draw_push();
    cf_draw_sprite(&layer->sprite);
    cf_draw_pop();
  }
}
