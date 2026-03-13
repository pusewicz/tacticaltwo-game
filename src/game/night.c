// night.c - Post-process night color grading
//
// Applies a fragment shader to the game canvas that simulates
// nighttime through blue-shift, desaturation, and darkening.

#include "night.h"

#include <cute_draw.h>
#include <cute_graphics.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/log.h"

void night_init(void) {
  NightState* night = &state->world.night;

  night->shader      = cf_make_draw_shader("night.shd");
  night->intensity   = NIGHT_INTENSITY;
  night->desaturation = NIGHT_DESATURATION;
  night->tint[0]     = NIGHT_TINT_R;
  night->tint[1]     = NIGHT_TINT_G;
  night->tint[2]     = NIGHT_TINT_B;
  night->initialized = true;

  log_info("night", "Night shader initialized (intensity=%.1f)",
           (double)night->intensity);
}

void night_push(void) {
  NightState* night = &state->world.night;
  if (!night->initialized || night->intensity <= 0.001f) {
    return;
  }

  cf_draw_push_shader(night->shader);
  cf_draw_set_uniform_float("u_intensity", night->intensity);
  cf_draw_set_uniform_float("u_desaturation", night->desaturation);
  cf_draw_set_uniform_float("u_tint_r", night->tint[0]);
  cf_draw_set_uniform_float("u_tint_g", night->tint[1]);
  cf_draw_set_uniform_float("u_tint_b", night->tint[2]);
}

void night_pop(void) {
  NightState* night = &state->world.night;
  if (!night->initialized || night->intensity <= 0.001f) {
    return;
  }

  cf_draw_pop_shader();
}

void night_shutdown(void) {
  NightState* night = &state->world.night;
  if (!night->initialized) {
    return;
  }

  cf_destroy_shader(night->shader);
  night->initialized = false;

  log_info("night", "Night shader shut down");
}
