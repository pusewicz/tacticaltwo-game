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
#define LDTK_GRID_SIZE 16

// =============================================================================
// Data Structures
// =============================================================================

typedef struct LdtkLayerImage {
  const char* identifier;
  CF_Sprite sprite;
  bool loaded;
} LdtkLayerImage;

#define LDTK_MAX_CUSTOM_FIELDS 8
#define LDTK_CUSTOM_FIELD_MAX_STR 32

typedef enum LdtkFieldType {
  LDTK_FIELD_NONE,
  LDTK_FIELD_INT,
  LDTK_FIELD_FLOAT,
  LDTK_FIELD_STRING,
} LdtkFieldType;

typedef struct LdtkCustomField {
  const char* key;
  LdtkFieldType type;
  union {
    int i;
    float f;
    char s[LDTK_CUSTOM_FIELD_MAX_STR];
  } value;
} LdtkCustomField;

typedef struct LdtkEntityInstance {
  const char* identifier;
  int x;
  int y;
  int width;
  int height;
  LdtkCustomField custom_fields[LDTK_MAX_CUSTOM_FIELDS];
  int custom_field_count;
} LdtkEntityInstance;

// Look up a custom field by key. Returns nullptr if not found.
const LdtkCustomField* ldtk_entity_get_field(const LdtkEntityInstance* e,
                                              const char* key);

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
