// night.h - Post-process night color grading
//
// Applies blue-shift, desaturation, and brightness reduction
// to the game canvas when drawing to screen.

#pragma once

#include <cute_graphics.h>
#include <stdbool.h>

typedef struct NightState {
  CF_Shader shader;
  float intensity;    // 0-1, how much night effect to apply
  float desaturation; // 0-1, how much color to remove
  float tint[3];      // RGB night tint color
  bool initialized;
} NightState;

// Load night shader and set defaults.
void night_init(void);

// Push night shader and set uniforms. Call before cf_draw_canvas.
void night_push(void);

// Pop night shader. Call after cf_draw_canvas.
void night_pop(void);

// Clean up GPU resources.
void night_shutdown(void);
