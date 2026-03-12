// world.h - Fat struct entity system
//
// Monolithic entity struct with embedded component data.
// Systems iterate a fixed array, filtering by enabled flags.

#pragma once

#include <cute.h>
#include <cute_coroutine.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <stdbool.h>

#include "ldtk.h"

// =============================================================================
// Constants
// =============================================================================

#define MAX_ENTITIES 4096
#define ENTITY_NONE -1

// =============================================================================
// Player State Enum
// =============================================================================

typedef enum PlayerState {
  PLAYER_STATE_IDLE,           // GunAim (stationary = aiming)
  PLAYER_STATE_WALKING,        // GunWalk
  PLAYER_STATE_CROUCHING,      // GunCrouch (stationary)
  PLAYER_STATE_CROUCH_WALKING, // GunCrouch (moving)
  PLAYER_STATE_FIRING,         // GunFire (one-shot animation)
  PLAYER_STATE_CROUCH_FIRING,  // GunCrouchFire (one-shot animation)
  PLAYER_STATE_RELOADING,      // GunReload
  PLAYER_STATE_COUNT
} PlayerState;

// =============================================================================
// Entity Components (embedded sub-structs)
// =============================================================================

typedef struct {
  bool enabled;
  bool up;
  bool down;
  bool left;
  bool right;
  bool crouch;
  bool shoot;
  bool reload;
} PlayerInput;

typedef struct {
  bool enabled;
  float walk_speed;
  CF_V2 facing_direction;
} PlayerController;

typedef struct {
  bool enabled;
  PlayerState current;
  CF_Coroutine co;
} PlayerStateComp;

typedef struct {
  bool enabled;
  CF_V2 position;
  float rotation;
} Transform;

typedef struct {
  bool enabled;
  CF_V2 value;
} Velocity;

typedef struct {
  bool enabled;
  CF_V2 half_size; // AABB half-extents from center
  CF_V2 offset;    // offset from transform position to collider center
  bool grounded;   // true when standing on solid tile
} Collider;

typedef struct {
  bool enabled;
  CF_Sprite sprite;
} Sprite;

// =============================================================================
// Entity - Fat struct containing all possible component data
// =============================================================================

typedef struct Entity {
  bool exists;

  PlayerInput player_input;
  PlayerController player_controller;
  PlayerStateComp player_state;
  Transform transform;
  Velocity velocity;
  Collider collider;
  Sprite sprite;
} Entity;

// =============================================================================
// World
// =============================================================================

typedef struct World {
  Entity entities[MAX_ENTITIES];
  float dt;
  int player; // index into entities[], ENTITY_NONE if no player
  CF_V2 camera;
  LdtkMap map;
} World;

// =============================================================================
// Entity Management
// =============================================================================

// Find a free slot and mark it as existing. Returns index, or ENTITY_NONE if
// full.
int world_add_entity(Entity e);

// Mark entity slot as free.
void world_remove_entity(int id);

// =============================================================================
// Player Factory
// =============================================================================

// Create a player entity at the given position.
void make_player_at(float x, float y);

// =============================================================================
// World Lifecycle
// =============================================================================

void init_world(void);
void world_hot_reload(void);
void update_world(float dt);
void render_world(void);
void shutdown_world(void);
