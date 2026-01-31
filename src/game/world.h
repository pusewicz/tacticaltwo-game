// world.h - ECS Components
//
// Player input and controller components for grounded tactical movement.

#pragma once

#include <cute.h>
#include <cute_hashtable.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <cute_string.h>
#include <pico_ecs.h>
#include <stdbool.h>

// =============================================================================
// World - ECS context with component/system registries
// =============================================================================

typedef struct World {
  ecs_t* ecs;
  htbl ecs_comp_t* components;
  htbl ecs_system_t* systems;
  float dt;
  ecs_entity_t player;
} World;

// =============================================================================
// ECS Macros
// =============================================================================

#ifndef ECS_ENTITY_COUNT
#define ECS_ENTITY_COUNT 4096
#endif

#define ECS_GET_COMP(COMP) hget(state->world.components, sintern(#COMP))

#define ECS_GET_SYSTEM(SYSTEM) hget(state->world.systems, sintern(#SYSTEM))

#define ECS_REGISTER_COMP(COMP)                                                \
  do {                                                                         \
    ecs_comp_t id = ecs_define_component(state->world.ecs, sizeof(COMP),       \
                                         nullptr, nullptr);                    \
    hset(state->world.components, sintern(#COMP), id);                         \
  } while (0)

#define ECS_REGISTER_COMP_CB(COMP, CTOR, DTOR)                                 \
  do {                                                                         \
    ecs_comp_t id =                                                            \
        ecs_define_component(state->world.ecs, sizeof(COMP), CTOR, DTOR);      \
    hset(state->world.components, sintern(#COMP), id);                         \
  } while (0)

#define ECS_REGISTER_SYSTEM(SYSTEM, UDATA)                                     \
  do {                                                                         \
    ecs_system_t id = ecs_define_system(state->world.ecs, 0, SYSTEM, nullptr,  \
                                        nullptr, UDATA);                       \
    hset(state->world.systems, sintern(#SYSTEM), id);                          \
  } while (0)

#define ECS_REQUIRE_COMP(SYSTEM, COMP)                                         \
  ecs_require_component(state->world.ecs, ECS_GET_SYSTEM(SYSTEM),              \
                        ECS_GET_COMP(COMP))

#define ECS_UPDATE_SYSTEM(SYSTEM, UDATA)                                       \
  ecs_set_system_callbacks(state->world.ecs, ECS_GET_SYSTEM(SYSTEM), SYSTEM,   \
                           nullptr, nullptr)

#define ECS_EXCLUDE_COMP(SYSTEM, COMP)                                         \
  ecs_exclude_component(state->world.ecs, ECS_GET_SYSTEM(SYSTEM),              \
                        ECS_GET_COMP(COMP))

#define ECS_GET(ENTITY, COMP)                                                  \
  (COMP*)ecs_get(state->world.ecs, ENTITY, ECS_GET_COMP(COMP))

#define ECS_ADD(ENTITY, COMP)                                                  \
  (COMP*)ecs_add(state->world.ecs, ENTITY, ECS_GET_COMP(COMP), nullptr)

#define ECS_RUN_SYSTEM(SYSTEM)                                                 \
  ecs_run_system(state->world.ecs, ECS_GET_SYSTEM(SYSTEM), 0)

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
// ECS Components
// =============================================================================

// C_PlayerInput - Action-based input component
// Captures player intentions from keyboard input.
// Movement actions use held state, action triggers are single-frame.
typedef struct C_PlayerInput {
  // Movement directions (held state)
  bool up;
  bool down;
  bool left;
  bool right;

  // Movement modifiers (held state)
  bool crouch; // Crouch/sneak mode

  // Action triggers (single-frame)
  bool shoot;
  bool reload;
} C_PlayerInput;

// C_PlayerController - Movement settings
// Configures player movement speeds and tracks facing direction.
typedef struct C_PlayerController {
  float walk_speed;
  CF_V2 facing_direction; // Normalized, (1,0) = right
} C_PlayerController;

// C_PlayerState - Simple state machine
// Tracks current player state for animation and behavior selection.
typedef struct C_PlayerState {
  PlayerState current;
  PlayerState previous;
  float state_timer;
} C_PlayerState;

// C_Transform - Position and rotation
// Stores entity position in world space.
typedef struct C_Transform {
  CF_V2 position;
  float rotation;
} C_Transform;

// C_Velocity - Movement vector
// Stores current velocity for physics integration.
typedef CF_V2 C_Velocity;

// C_Sprite - Sprite and animation component
// Holds sprite data for rendering and animation.
typedef CF_Sprite C_Sprite;

// =============================================================================
// Function Declarations
// =============================================================================

// Initialize world ECS and spawn player
void init_world(void);

// Update system callbacks after hot reload
void world_hot_reload(void);

// Main update function - runs all systems in order
void update_world(float dt);

// Render all sprites
void render_world(void);

// Cleanup world resources
void shutdown_world(void);

// Player factory function (creates entity and returns nothing - entity is
// internal)
void make_player(void);
