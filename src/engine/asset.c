#include "asset.h"

#include <cute_c_runtime.h>
#include <cute_file_system.h>
#include <cute_result.h>
#include <cute_sprite.h>

CF_Result asset_load_sprite(const char* filepath, CF_Sprite* out_sprite) {
  CF_ASSERT(out_sprite != nullptr);

  *out_sprite = cf_sprite_defaults();

  if (!filepath) {
    return cf_result_error("filepath is NULL");
  }

  if (spext_equ(filepath, ".aseprite") || spext_equ(filepath, ".ase")) {
    *out_sprite = cf_make_sprite(filepath);
    if (!out_sprite->name) {
      return cf_result_error("Failed to load Aseprite sprite");
    }
    return cf_result_success();
  }

  if (spext_equ(filepath, ".png")) {
    CF_Result result;
    *out_sprite = cf_make_easy_sprite_from_png(filepath, &result);
    return result;
  }

  return cf_result_error("Unsupported sprite file format");
}
