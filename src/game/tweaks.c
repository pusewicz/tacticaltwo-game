// tweaks.c - Save/load ImGui debug tweaks to JSON

#include "tweaks.h"

#include <cute_file_system.h>
#include <cute_json.h>
#include <cute_string.h>
#include <stdio.h>

#include "../engine/game_state.h"
#include "../engine/log.h"
#include "rain.h"
#include "world.h"

#define TWEAKS_VIRTUAL_PATH "/assets/tweaks.json"

#ifdef ASSETS_PATH
#define TWEAKS_WRITE_PATH ASSETS_PATH "/tweaks.json"
#endif

// Helper: read a float from a JSON object, returning fallback if key is missing.
static float json_get_float_or(CF_JVal obj, const char* key, float fallback) {
  CF_JVal val = cf_json_get(obj, key);
  if (cf_json_type(val) == CF_JTYPE_NONE) {
    return fallback;
  }
  return cf_json_get_float(val);
}

void tweaks_save(void) {
#ifndef ASSETS_PATH
  log_error("tweaks", "Cannot save: ASSETS_PATH not defined (release build)");
#else
  World* w = &state->world;

  CF_JDoc doc = cf_make_json(nullptr, 0);
  CF_JVal root = cf_json_object(doc);
  cf_json_set_root(doc, root);

  // Rain
  CF_JVal rain = cf_json_object(doc);
  cf_json_object_add(doc, root, "rain", rain);
  cf_json_object_add_float(doc, rain, "intensity", w->rain.intensity);
  cf_json_object_add_float(doc, rain, "density", w->rain.density);
  cf_json_object_add_float(doc, rain, "alpha", w->rain.alpha);
  cf_json_object_add_float(doc, rain, "speed", w->rain.speed);
  cf_json_object_add_float(doc, rain, "wind", w->rain.wind);
  cf_json_object_add_float(doc, rain, "splash_cell_size", w->rain.splash_cell_size);
  cf_json_object_add_float(doc, rain, "splash_rate", w->rain.splash_rate);
  cf_json_object_add_float(doc, rain, "splash_life", w->rain.splash_life);
  cf_json_object_add_float(doc, rain, "splash_speed", w->rain.splash_speed);
  cf_json_object_add_float(doc, rain, "splash_gravity", w->rain.splash_gravity);
  cf_json_object_add_float(doc, rain, "max_volume", w->rain.max_volume);
  cf_json_object_add_float(doc, rain, "cutoff_hz", w->rain.cutoff_hz);

  // Night
  CF_JVal night = cf_json_object(doc);
  cf_json_object_add(doc, root, "night", night);
  cf_json_object_add_float(doc, night, "intensity", w->night.intensity);
  cf_json_object_add_float(doc, night, "desaturation", w->night.desaturation);
  cf_json_object_add_float(doc, night, "tint_r", w->night.tint[0]);
  cf_json_object_add_float(doc, night, "tint_g", w->night.tint[1]);
  cf_json_object_add_float(doc, night, "tint_b", w->night.tint[2]);

  char* json_str = cf_json_to_string(doc);
  FILE* f = fopen(TWEAKS_WRITE_PATH, "w");
  if (f) {
    fputs(json_str, f);
    fclose(f);
    log_info("tweaks", "Saved tweaks to %s", TWEAKS_WRITE_PATH);
  } else {
    log_error("tweaks", "Failed to open %s for writing", TWEAKS_WRITE_PATH);
  }
  sfree(json_str);
  cf_destroy_json(doc);
#endif
}

void tweaks_load(void) {
  if (!cf_fs_file_exists(TWEAKS_VIRTUAL_PATH)) {
    log_debug("tweaks", "No tweaks file found, using defaults");
    return;
  }

  CF_JDoc doc = cf_make_json_from_file(TWEAKS_VIRTUAL_PATH);
  CF_JVal root = cf_json_get_root(doc);
  if (cf_json_type(root) != CF_JTYPE_OBJECT) {
    log_error("tweaks", "Invalid tweaks file (root is not an object)");
    cf_destroy_json(doc);
    return;
  }

  World* w = &state->world;

  // Rain
  CF_JVal rain = cf_json_get(root, "rain");
  if (cf_json_type(rain) == CF_JTYPE_OBJECT) {
    w->rain.intensity       = json_get_float_or(rain, "intensity", w->rain.intensity);
    w->rain.density         = json_get_float_or(rain, "density", w->rain.density);
    w->rain.alpha           = json_get_float_or(rain, "alpha", w->rain.alpha);
    w->rain.speed           = json_get_float_or(rain, "speed", w->rain.speed);
    w->rain.wind            = json_get_float_or(rain, "wind", w->rain.wind);
    w->rain.splash_cell_size = json_get_float_or(rain, "splash_cell_size", w->rain.splash_cell_size);
    w->rain.splash_rate     = json_get_float_or(rain, "splash_rate", w->rain.splash_rate);
    w->rain.splash_life     = json_get_float_or(rain, "splash_life", w->rain.splash_life);
    w->rain.splash_speed    = json_get_float_or(rain, "splash_speed", w->rain.splash_speed);
    w->rain.splash_gravity  = json_get_float_or(rain, "splash_gravity", w->rain.splash_gravity);
    w->rain.max_volume      = json_get_float_or(rain, "max_volume", w->rain.max_volume);

    float prev_cutoff = w->rain.cutoff_hz;
    w->rain.cutoff_hz = json_get_float_or(rain, "cutoff_hz", w->rain.cutoff_hz);
    if (w->rain.cutoff_hz != prev_cutoff) {
      rain_rebuild_audio();
    }
  }

  // Night
  CF_JVal night = cf_json_get(root, "night");
  if (cf_json_type(night) == CF_JTYPE_OBJECT) {
    w->night.intensity    = json_get_float_or(night, "intensity", w->night.intensity);
    w->night.desaturation = json_get_float_or(night, "desaturation", w->night.desaturation);
    w->night.tint[0]      = json_get_float_or(night, "tint_r", w->night.tint[0]);
    w->night.tint[1]      = json_get_float_or(night, "tint_g", w->night.tint[1]);
    w->night.tint[2]      = json_get_float_or(night, "tint_b", w->night.tint[2]);
  }

  cf_destroy_json(doc);
  log_info("tweaks", "Loaded tweaks from %s", TWEAKS_VIRTUAL_PATH);
}
