// ldtk.c - LDtk simplified export loader
//
// Parses data.json, loads pre-rendered layer PNGs, parses IntGrid CSVs.

#include "ldtk.h"

#include <cute.h>
#include <cute_alloc.h>
#include <cute_coroutine.h>
#include <cute_file_system.h>
#include <cute_json.h>
#include <cute_result.h>
#include <cute_sprite.h>
#include <cute_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../engine/game_state.h"
#include "../engine/log.h"
#include "world.h"

#define LDTK_TAG "ldtk"
#define LDTK_GRID_SIZE 16
#define LDTK_RELOAD_INTERVAL 30

// =============================================================================
// Internal Helpers
// =============================================================================

static void build_path(char* out, size_t out_size, const char* base,
                       const char* sub) {
  snprintf(out, out_size, "%s/%s", base, sub);
}

static bool parse_csv(const char* virtual_path, int** out_grid, int* out_width,
                      int* out_height) {
  size_t size = 0;
  char* data =
      cf_fs_read_entire_file_to_memory_and_nul_terminate(virtual_path, &size);
  if (!data) {
    log_error(LDTK_TAG, "Failed to read CSV: %s", virtual_path);
    return false;
  }

  // First pass: count rows and columns
  int rows         = 0;
  int cols         = 0;
  int current_cols = 0;
  for (size_t i = 0; i < size; i++) {
    if (data[i] == ',') {
      current_cols++;
    } else if (data[i] == '\n') {
      if (current_cols > 0 || (i > 0 && data[i - 1] != '\n')) {
        current_cols++; // last value before newline
        if (cols == 0) {
          cols = current_cols;
        }
        rows++;
      }
      current_cols = 0;
    }
  }
  // Handle last row without trailing newline
  if (current_cols > 0) {
    current_cols++;
    if (cols == 0) {
      cols = current_cols;
    }
    rows++;
  }

  if (rows == 0 || cols == 0) {
    cf_free(data);
    return false;
  }

  int* grid = (int*)cf_alloc(sizeof(int) * (size_t)(rows * cols));
  int idx   = 0;
  char* p   = data;
  while (*p) {
    if (*p == '\n' || *p == '\r') {
      p++;
      continue;
    }
    char* end = nullptr;
    long val  = strtol(p, &end, 10);
    if (end == p) {
      p++;
      continue;
    }
    if (idx < rows * cols) {
      grid[idx++] = (int)val;
    }
    p = end;
    if (*p == ',') {
      p++;
    }
  }

  cf_free(data);

  *out_grid   = grid;
  *out_width  = cols;
  *out_height = rows;
  return true;
}

static bool parse_level_data(LdtkMap* map, const char* level_dir,
                             int level_index) {
  LdtkLevel* level = &map->levels[level_index];

  char data_path[LDTK_MAX_PATH];
  build_path(data_path, sizeof(data_path), level_dir, "data.json");

  CF_JDoc doc  = cf_make_json_from_file(data_path);
  CF_JVal root = cf_json_get_root(doc);
  if (cf_json_type(root) == CF_JTYPE_NONE) {
    log_error(LDTK_TAG, "Failed to parse %s", data_path);
    cf_destroy_json(doc);
    return false;
  }

  // Level metadata
  level->identifier =
      cf_sintern(cf_json_get_string(cf_json_get(root, "identifier")));
  level->x      = cf_json_get_int(cf_json_get(root, "x"));
  level->y      = cf_json_get_int(cf_json_get(root, "y"));
  level->width  = cf_json_get_int(cf_json_get(root, "width"));
  level->height = cf_json_get_int(cf_json_get(root, "height"));

  // Layers (array of PNG filenames)
  CF_JVal layers_val = cf_json_get(root, "layers");
  level->layer_count = 0;
  if (cf_json_is_array(layers_val)) {
    for (CF_JIter it = cf_json_iter(layers_val); !cf_json_iter_done(it);
         it          = cf_json_iter_next(it)) {
      if (level->layer_count >= LDTK_MAX_LAYERS) {
        break;
      }
      const char* png_name = cf_json_get_string(cf_json_iter_val(it));
      level->layers[level->layer_count].identifier = cf_sintern(png_name);

      char png_path[LDTK_MAX_PATH];
      build_path(png_path, sizeof(png_path), level_dir, png_name);

      CF_Result result = {0};
      level->layers[level->layer_count].sprite =
          cf_make_easy_sprite_from_png(png_path, &result);
      if (cf_is_error(result)) {
        log_error(LDTK_TAG, "Failed to load layer PNG: %s", png_path);
        level->layers[level->layer_count].loaded = false;
      } else {
        level->layers[level->layer_count].loaded = true;
      }
      level->layer_count++;
    }
  }

  // Entities (object keyed by entity identifier)
  CF_JVal entities_val = cf_json_get(root, "entities");
  level->entity_count  = 0;
  if (cf_json_is_object(entities_val)) {
    for (CF_JIter eit = cf_json_iter(entities_val); !cf_json_iter_done(eit);
         eit          = cf_json_iter_next(eit)) {
      const char* entity_type = cf_json_iter_key(eit);
      CF_JVal instances       = cf_json_iter_val(eit);
      if (!cf_json_is_array(instances)) {
        continue;
      }
      for (CF_JIter iit = cf_json_iter(instances); !cf_json_iter_done(iit);
           iit          = cf_json_iter_next(iit)) {
        if (level->entity_count >= LDTK_MAX_ENTITIES) {
          break;
        }
        CF_JVal inst          = cf_json_iter_val(iit);
        LdtkEntityInstance* e = &level->entities[level->entity_count];
        e->identifier         = cf_sintern(entity_type);
        e->x                  = cf_json_get_int(cf_json_get(inst, "x"));
        e->y                  = cf_json_get_int(cf_json_get(inst, "y"));
        e->width              = cf_json_get_int(cf_json_get(inst, "width"));
        e->height             = cf_json_get_int(cf_json_get(inst, "height"));
        level->entity_count++;
      }
    }
  }

  cf_destroy_json(doc);

  // Parse IntGrid CSV files (look for *.csv)
  // LDtk simplified export names them like "Floor.csv"
  level->int_grid    = nullptr;
  level->grid_width  = 0;
  level->grid_height = 0;

  // Try common IntGrid layer name
  char csv_path[LDTK_MAX_PATH];
  build_path(csv_path, sizeof(csv_path), level_dir, "Floor.csv");

  CF_Stat csv_stat;
  CF_Result stat_result = cf_fs_stat(csv_path, &csv_stat);
  if (!cf_is_error(stat_result) && csv_stat.type == CF_FILE_TYPE_REGULAR) {
    parse_csv(csv_path, &level->int_grid, &level->grid_width,
              &level->grid_height);
    log_info(LDTK_TAG, "Loaded IntGrid %dx%d from %s", level->grid_width,
             level->grid_height, csv_path);
  }

  log_info(LDTK_TAG, "Loaded level '%s' (%dx%d) with %d layers, %d entities",
           level->identifier, level->width, level->height, level->layer_count,
           level->entity_count);

  return true;
}

// =============================================================================
// Public API
// =============================================================================

bool ldtk_load(LdtkMap* map, const char* simplified_base_path) {
  memset(map, 0, sizeof(LdtkMap));
  snprintf(map->base_path, sizeof(map->base_path), "%s", simplified_base_path);

  // Discover levels by trying Level_0, Level_1, etc.
  map->level_count = 0;
  for (int i = 0; i < LDTK_MAX_LEVELS; i++) {
    char level_dir[LDTK_MAX_PATH];
    snprintf(level_dir, sizeof(level_dir), "%s/Level_%d", simplified_base_path,
             i);

    char data_path[LDTK_MAX_PATH];
    build_path(data_path, sizeof(data_path), level_dir, "data.json");

    CF_Stat file_stat;
    CF_Result result = cf_fs_stat(data_path, &file_stat);
    if (cf_is_error(result)) {
      break; // No more levels
    }

    if (!parse_level_data(map, level_dir, map->level_count)) {
      log_error(LDTK_TAG, "Failed to parse level at %s", level_dir);
      continue;
    }

    // Record modification time from first level for hot-reload detection
    if (map->level_count == 0) {
      map->last_modified_time = file_stat.last_modified_time;
    }

    map->level_count++;
  }

  if (map->level_count == 0) {
    log_error(LDTK_TAG, "No levels found at %s", simplified_base_path);
    return false;
  }

  map->active_level   = 0;
  map->loaded         = true;
  map->reload_counter = 0;

  log_info(LDTK_TAG, "Loaded %d level(s) from %s", map->level_count,
           simplified_base_path);
  return true;
}

void ldtk_unload(LdtkMap* map) {
  for (int i = 0; i < map->level_count; i++) {
    LdtkLevel* level = &map->levels[i];

    for (int j = 0; j < level->layer_count; j++) {
      if (level->layers[j].loaded) {
        cf_easy_sprite_unload(&level->layers[j].sprite);
        level->layers[j].loaded = false;
      }
    }

    if (level->int_grid) {
      cf_free(level->int_grid);
      level->int_grid = nullptr;
    }
  }

  map->level_count = 0;
  map->loaded      = false;
}

bool ldtk_check_reload(LdtkMap* map) {
  if (!map->loaded) {
    return false;
  }

  map->reload_counter++;
  if (map->reload_counter < LDTK_RELOAD_INTERVAL) {
    return false;
  }
  map->reload_counter = 0;

  char data_path[LDTK_MAX_PATH];
  snprintf(data_path, sizeof(data_path), "%s/Level_0/data.json",
           map->base_path);

  CF_Stat file_stat;
  CF_Result result = cf_fs_stat(data_path, &file_stat);
  if (cf_is_error(result)) {
    return false;
  }

  if (file_stat.last_modified_time <= map->last_modified_time) {
    return false;
  }

  log_info(LDTK_TAG, "LDtk map changed, reloading...");

  char base_path[LDTK_MAX_PATH];
  snprintf(base_path, sizeof(base_path), "%s", map->base_path);

  ldtk_unload(map);

  // Clear all entities before respawning
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (e->exists && e->player_state.enabled && e->player_state.co.id != 0) {
      cf_destroy_coroutine(e->player_state.co);
    }
  }
  memset(state->world.entities, 0, sizeof(state->world.entities));
  state->world.player = ENTITY_NONE;

  if (!ldtk_load(map, base_path)) {
    return false;
  }

  ldtk_spawn_entities(map, map->active_level);
  return true;
}

void ldtk_spawn_entities(LdtkMap* map, int level_index) {
  if (level_index < 0 || level_index >= map->level_count) {
    return;
  }

  LdtkLevel* level = &map->levels[level_index];

  for (int i = 0; i < level->entity_count; i++) {
    LdtkEntityInstance* ent = &level->entities[i];

    if (strcmp(ent->identifier, "Player") == 0) {
      // Convert LDtk coords (Y-down, top-left origin) to CF coords (Y-up,
      // center origin)
      float cf_x = (float)ent->x - (float)level->width / 2.0f;
      float cf_y = (float)level->height / 2.0f - (float)ent->y;
      make_player_at(cf_x, cf_y);
      log_info(LDTK_TAG, "Spawned Player at LDtk(%d, %d) -> CF(%.0f, %.0f)",
               ent->x, ent->y, (double)cf_x, (double)cf_y);
    }
  }
}

int ldtk_get_intgrid(LdtkMap* map, int level_index, int grid_x, int grid_y) {
  if (level_index < 0 || level_index >= map->level_count) {
    return 0;
  }

  LdtkLevel* level = &map->levels[level_index];
  if (!level->int_grid) {
    return 0;
  }

  if (grid_x < 0 || grid_x >= level->grid_width || grid_y < 0 ||
      grid_y >= level->grid_height) {
    return 0;
  }

  return level->int_grid[grid_y * level->grid_width + grid_x];
}
