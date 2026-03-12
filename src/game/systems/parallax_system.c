// parallax_system.c - Parallax scrolling background layers
//
// Draws tiling background layers that scroll at different rates
// relative to the camera. Must be called outside the camera translate.

#include <cute_draw.h>
#include <cute_sprite.h>
#include <math.h>

#include "../../config/config.h"
#include "../../engine/game_state.h"
#include "../world.h"
#include "systems.h"

typedef struct {
  CF_Sprite* sprite;
  float scroll_x;
  float scroll_y;
  float img_w;
  float img_h;
} ParallaxLayer;

static void draw_layer(ParallaxLayer* layer, CF_V2 camera) {
  float half_canvas_h = (float)CANVAS_HEIGHT / 2.0f;

  // Horizontal: wrap offset and tile 3 copies (1024 > 720, so 3 always covers)
  float offset_x = fmodf(-camera.x * layer->scroll_x, layer->img_w);

  // Vertical: bottom-aligned base + parallax offset
  float base_y   = -half_canvas_h + layer->img_h / 2.0f;
  float offset_y = base_y - camera.y * layer->scroll_y;

  for (int t = -1; t <= 1; t++) {
    float x = offset_x + (float)t * layer->img_w;
    cf_draw_push();
    cf_draw_translate(x, offset_y);
    cf_draw_sprite(layer->sprite);
    cf_draw_pop();
  }
}

void sys_render_parallax(void) {
  Parallax* p = &state->world.parallax;
  if (!p->loaded) {
    return;
  }

  CF_V2 camera = state->world.camera;

  ParallaxLayer layers[3] = {
      {&p->sky, 0.05f, 0.02f, 1024.0f, 346.0f},
      {&p->layer1, 0.3f, 0.15f, 1024.0f, 346.0f},
      {&p->layer2, 0.6f, 0.3f, 1024.0f, 346.0f},
  };

  for (int i = 0; i < 3; i++) {
    draw_layer(&layers[i], camera);
  }
}
