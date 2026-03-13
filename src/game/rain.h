// rain.h - Procedural rain storm effect
//
// Fullscreen shader overlay with world-space rain streaks and
// ground splashes that interact with the IntGrid tilemap.

#pragma once

#include <cute_graphics.h>
#include <stdbool.h>

typedef struct RainState {
  CF_Shader shader;
  CF_Texture collision_mask; // 2D IntGrid: R=255 solid, R=0 empty
  CF_Texture height_map;     // 1D: R = topmost solid row per column
  float intensity;           // 0-1, atmospheric tint strength
  float density;             // 0-1, fraction of cells that contain a drop
  float alpha;               // 0-1, max opacity of rain streaks
  float speed;               // multiplier for fall speed
  float wind;                // wind angle: -1 left, 0 straight, +1 right
  float time;                // accumulated elapsed time
  int mask_width;
  int mask_height;
  bool initialized;
  bool enabled;
  float splash_cell_size;    // spacing between splash origins (pixels)
  float splash_rate;         // bursts per second per cell
  float splash_life;         // duration of each burst (seconds)
  float splash_speed;        // particle travel speed
  float splash_gravity;      // how quickly particles arc down
} RainState;

// Initialize rain shader and height map from current map.
void rain_init(void);

// Rebuild the ground height map (call after map reload).
void rain_rebuild_height_map(void);

// Render rain overlay. Call after world geometry, in canvas space (no camera transform).
void rain_render(float dt);

// Clean up GPU resources.
void rain_shutdown(void);
