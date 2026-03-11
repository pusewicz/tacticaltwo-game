// ldtk.h - LDtk simplified export loader
//
// Parses LDtk simplified export format: data.json for metadata/entities,
// pre-rendered PNGs for tile layers, CSV files for IntGrid collision.

#pragma once

#include <cute_sprite.h>
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Constants
// =============================================================================

#define LDTK_MAX_LEVELS 16
#define LDTK_MAX_LAYERS 8
#define LDTK_MAX_ENTITIES 64
#define LDTK_MAX_PATH 256

// =============================================================================
// Data Structures
// =============================================================================

typedef struct LdtkLayerImage {
  const char* identifier;
  CF_Sprite sprite;
  bool loaded;
} LdtkLayerImage;

typedef struct LdtkEntityInstance {
  const char* identifier;
  int x;
  int y;
  int width;
  int height;
} LdtkEntityInstance;

typedef struct LdtkLevel {
  const char* identifier;
  int x;
  int y;
  int width;
  int height;

  LdtkLayerImage layers[LDTK_MAX_LAYERS];
  int layer_count;

  LdtkEntityInstance entities[LDTK_MAX_ENTITIES];
  int entity_count;

  int* int_grid;
  int grid_width;
  int grid_height;
} LdtkLevel;

typedef struct LdtkMap {
  char base_path[LDTK_MAX_PATH];
  uint64_t last_modified_time;
  int reload_counter;

  LdtkLevel levels[LDTK_MAX_LEVELS];
  int level_count;
  int active_level;

  bool loaded;
} LdtkMap;

// =============================================================================
// Public API
// =============================================================================

// Load all levels from the simplified export directory.
// base_path is a CF virtual path, e.g. "/assets/ldtk/map/simplified"
bool ldtk_load(LdtkMap* map, const char* simplified_base_path);

// Unload all level data and free resources.
void ldtk_unload(LdtkMap* map);

// Poll data.json modification time; reload if changed.
// Call from update loop. Returns true if a reload occurred.
bool ldtk_check_reload(LdtkMap* map);

// Spawn game entities from the active level's entity data.
void ldtk_spawn_entities(LdtkMap* map, int level_index);

// Query IntGrid value at grid coordinates. Returns 0 for OOB.
int ldtk_get_intgrid(LdtkMap* map, int level_index, int grid_x, int grid_y);
