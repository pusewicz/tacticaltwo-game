// world.c - ECS Systems for Tactical Side-Scroller
//
// Systems for player input, state management, and movement.
// Uses pico_ecs library for entity-component-system architecture.

#define PICO_ECS_IMPLEMENTATION

#include "world.h"
#include "coro.h"

// =============================================================================
// Animation Constants
// =============================================================================

// GunWalkFire animation has 8 frames but we only play the first few for a single shot
// Stop when current frame reaches this value (0-indexed frames: 0, 1, 2 are played)
#define GUNWALKFIRE_SINGLE_SHOT_STOP_FRAME 3

#include <cute_draw.h>
#include <cute_hashtable.h>
#include <cute_input.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <stddef.h>

#include "../engine/game_state.h"

// =============================================================================
// System: Gather Input
// =============================================================================
// Reads keyboard state and populates the input component.
// Movement uses held state, actions use single-frame triggers.

static ecs_ret_t sys_gather_input([[maybe_unused]] ecs_t* ecs,
                                  ecs_entity_t* entities, size_t count,
                                  [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; ++i) {
    auto input = ECS_GET(entities[i], C_PlayerInput);

    // Movement directions (held state)
    input->up    = cf_key_down(CF_KEY_W) || cf_key_down(CF_KEY_UP);
    input->down  = cf_key_down(CF_KEY_S) || cf_key_down(CF_KEY_DOWN);
    input->left  = cf_key_down(CF_KEY_A) || cf_key_down(CF_KEY_LEFT);
    input->right = cf_key_down(CF_KEY_D) || cf_key_down(CF_KEY_RIGHT);

    // Movement modifiers (held state)
    input->crouch = cf_key_down(CF_KEY_LCTRL);

    // Action triggers (single-frame)
    input->shoot  = cf_mouse_just_pressed(CF_MOUSE_BUTTON_LEFT);
    input->reload = cf_key_just_pressed(CF_KEY_R);
  }

  return 0;
}

// =============================================================================
// System: Update Player State
// =============================================================================
// Determines player state based on input.
// Priority: Aiming > Crouch Walking > Crouching > Walking > Idle

static ecs_ret_t sys_update_player_state([[maybe_unused]] ecs_t* ecs,
                                         ecs_entity_t* entities, size_t count,
                                         [[maybe_unused]] void* udata) {
  float dt = state->world.dt;

  for (size_t i = 0; i < count; ++i) {
    auto ps    = ECS_GET(entities[i], C_PlayerState);
    auto input = ECS_GET(entities[i], C_PlayerInput);

    // Check if player is moving horizontally
    bool moving = input->left || input->right;

    // Store previous state for transition detection
    ps->previous = ps->current;

    // Lock state during reloading or firing - wait for animation to finish
    if (ps->current == PLAYER_STATE_RELOADING ||
        ps->current == PLAYER_STATE_FIRING ||
        ps->current == PLAYER_STATE_CROUCH_FIRING) {
      ps->state_timer += dt;
      continue;
    }

    // Determine new state based on input priority
    // Firing state triggered by shoot input (crouch firing if crouching)
    if (input->shoot && input->crouch) {
      ps->current = PLAYER_STATE_CROUCH_FIRING;
    } else if (input->shoot) {
      ps->current = PLAYER_STATE_FIRING;
    } else if (input->reload) {
      ps->current = PLAYER_STATE_RELOADING;
    } else if (input->crouch) {
      ps->current = PLAYER_STATE_CROUCHING;
    } else if (moving) {
      ps->current = PLAYER_STATE_WALKING;
    } else {
      ps->current = PLAYER_STATE_IDLE;
    }

    // Update state timer and reset coroutine on state change
    if (ps->current != ps->previous) {
      ps->state_timer = 0.0f;
      ps->coroutine_state = 0; // Reset coroutine for new state
    } else {
      ps->state_timer += dt;
    }
  }

  return 0;
}

// =============================================================================
// System: Update Player Movement
// =============================================================================
// Sets velocity based on state and input.
// Speed varies by state: walk > crouch > aim

static ecs_ret_t sys_update_player_movement([[maybe_unused]] ecs_t* ecs,
                                            ecs_entity_t* entities,
                                            size_t count,
                                            [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto velocity   = ECS_GET(entities[i], C_Velocity);
    auto controller = ECS_GET(entities[i], C_PlayerController);
    auto ps         = ECS_GET(entities[i], C_PlayerState);
    auto input      = ECS_GET(entities[i], C_PlayerInput);

    // No movement while crouching
    if (ps->current == PLAYER_STATE_CROUCHING ||
        ps->current == PLAYER_STATE_CROUCH_FIRING) {
      velocity->x = 0.0f;
      velocity->y = 0.0f;
    } else {
      // Calculate horizontal velocity
      velocity->x = 0.0f;
      if (input->left) {
        velocity->x -= controller->walk_speed;
      }
      if (input->right) {
        velocity->x += controller->walk_speed;
      }

      // No vertical movement for side-scroller (no jumping initially)
      velocity->y = 0.0f;
    }

    // Update facing direction based on input
    if (input->right && !input->left) {
      controller->facing_direction = cf_v2(1.0f, 0.0f);
    } else if (input->left && !input->right) {
      controller->facing_direction = cf_v2(-1.0f, 0.0f);
    }
  }

  return 0;
}

// =============================================================================
// System: Apply Velocity
// =============================================================================
// Integrates velocity into position using simple Euler integration.

static ecs_ret_t sys_apply_velocity([[maybe_unused]] ecs_t* ecs,
                                    ecs_entity_t* entities, size_t count,
                                    [[maybe_unused]] void* udata) {
  float dt = state->world.dt;

  for (size_t i = 0; i < count; i++) {
    auto transform = ECS_GET(entities[i], C_Transform);
    auto velocity  = ECS_GET(entities[i], C_Velocity);

    // Simple Euler integration: position += velocity * dt
    transform->position = cf_add(transform->position, cf_mul(*velocity, dt));
  }

  return 0;
}

// =============================================================================
// Animation State Handlers - Stackless Coroutines
// =============================================================================
// Each handler is a resumable coroutine that manages one animation state.
// They can yield execution while waiting for animations to complete.

// Handler for IDLE state
static int anim_state_idle(C_Sprite* sprite_comp, C_PlayerState* ps,
                          C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Play idle animation
  if (!cf_sprite_is_playing(sprite_comp, "GunAim")) {
    cf_sprite_play(sprite_comp, "GunAim");
  }
  
  // Infinite loop - yield every frame until state changes externally
  // State transition handled by sys_update_player_state
  while (true) {
    coro_yield(ps->coroutine_state);
  }
  
  coro_end(ps->coroutine_state);
}

// Handler for WALKING state
static int anim_state_walking(C_Sprite* sprite_comp, C_PlayerState* ps,
                             C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Play walk animation
  if (!cf_sprite_is_playing(sprite_comp, "GunWalk")) {
    cf_sprite_play(sprite_comp, "GunWalk");
  }
  
  // Infinite loop - yield every frame until state changes externally
  // State transition handled by sys_update_player_state
  while (true) {
    coro_yield(ps->coroutine_state);
  }
  
  coro_end(ps->coroutine_state);
}

// Handler for CROUCHING state
static int anim_state_crouching(C_Sprite* sprite_comp, C_PlayerState* ps,
                               C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Play crouch animation
  if (!cf_sprite_is_playing(sprite_comp, "GunCrouch")) {
    cf_sprite_play(sprite_comp, "GunCrouch");
  }
  
  // Infinite loop - yield every frame until state changes externally
  // State transition handled by sys_update_player_state
  while (true) {
    coro_yield(ps->coroutine_state);
  }
  
  coro_end(ps->coroutine_state);
}

// Handler for FIRING state
static int anim_state_firing(C_Sprite* sprite_comp, C_PlayerState* ps,
                            C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Entry: Select appropriate fire animation based on velocity
  if (!cf_sprite_is_playing(sprite_comp, "GunFire") &&
      !cf_sprite_is_playing(sprite_comp, "GunWalkFire")) {
    bool moving = velocity->x != 0.0f;
    const char* anim = moving ? "GunWalkFire" : "GunFire";
    cf_sprite_play(sprite_comp, anim);
  }
  
  // Wait one frame to let state_timer increment
  coro_yield(ps->coroutine_state);
  
  // Wait for animation to complete
  while (ps->state_timer > 0.0f) {
    bool should_finish = false;
    if (cf_sprite_is_playing(sprite_comp, "GunWalkFire")) {
      // Stop GunWalkFire early to play only a single shot
      should_finish = cf_sprite_current_frame(sprite_comp) >= 
                      GUNWALKFIRE_SINGLE_SHOT_STOP_FRAME;
    } else {
      should_finish = cf_sprite_will_finish(sprite_comp);
    }
    
    if (should_finish) {
      // Transition to IDLE (coroutine_state reset by sys_update_player_state)
      ps->current = PLAYER_STATE_IDLE;
      coro_end(ps->coroutine_state);
    }
    
    coro_yield(ps->coroutine_state);
  }
  
  coro_end(ps->coroutine_state);
}

// Handler for CROUCH_FIRING state
static int anim_state_crouch_firing(C_Sprite* sprite_comp, C_PlayerState* ps,
                                   C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Play crouch fire animation
  if (!cf_sprite_is_playing(sprite_comp, "GunCrouchFire")) {
    cf_sprite_play(sprite_comp, "GunCrouchFire");
  }
  
  // Wait one frame to let state_timer increment
  coro_yield(ps->coroutine_state);
  
  // Wait for animation to complete
  while (ps->state_timer > 0.0f && !cf_sprite_will_finish(sprite_comp)) {
    coro_yield(ps->coroutine_state);
  }
  
  // Transition to CROUCHING (coroutine_state reset by sys_update_player_state)
  ps->current = PLAYER_STATE_CROUCHING;
  
  coro_end(ps->coroutine_state);
}

// Handler for RELOADING state
static int anim_state_reloading(C_Sprite* sprite_comp, C_PlayerState* ps,
                               C_PlayerController* controller, C_Velocity* velocity) {
  coro_begin(ps->coroutine_state);
  
  // Play reload animation
  if (!cf_sprite_is_playing(sprite_comp, "GunReload")) {
    cf_sprite_play(sprite_comp, "GunReload");
  }
  
  // Wait one frame to let state_timer increment
  coro_yield(ps->coroutine_state);
  
  // Wait for animation to complete
  while (ps->state_timer > 0.0f && !cf_sprite_will_finish(sprite_comp)) {
    coro_yield(ps->coroutine_state);
  }
  
  // Transition to IDLE (coroutine_state reset by sys_update_player_state)
  ps->current = PLAYER_STATE_IDLE;
  
  coro_end(ps->coroutine_state);
}

// =============================================================================
// System: Update Animation
// =============================================================================
// Stackless coroutine-based animation system using Duff's Device.
// Each state is handled by a resumable coroutine that can yield execution
// while waiting for animations to complete. This provides:
//   - Trivial serialization (save/load game state)
//   - Zero memory overhead (execution state in coroutine_state)
//   - Manual variable persistence (all state in component structs)

// NOLINTBEGIN
static ecs_ret_t sys_update_animation([[maybe_unused]] ecs_t* ecs,
                                      ecs_entity_t* entities, size_t count,
                                      [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto sprite_comp = ECS_GET(entities[i], C_Sprite);
    auto ps          = ECS_GET(entities[i], C_PlayerState);
    auto controller  = ECS_GET(entities[i], C_PlayerController);
    auto velocity    = ECS_GET(entities[i], C_Velocity);

    // Dispatch to appropriate coroutine based on current state
    switch (ps->current) {
    case PLAYER_STATE_IDLE:
      anim_state_idle(sprite_comp, ps, controller, velocity);
      break;

    case PLAYER_STATE_WALKING:
      anim_state_walking(sprite_comp, ps, controller, velocity);
      break;

    case PLAYER_STATE_CROUCHING:
    case PLAYER_STATE_CROUCH_WALKING:
      anim_state_crouching(sprite_comp, ps, controller, velocity);
      break;

    case PLAYER_STATE_FIRING:
      anim_state_firing(sprite_comp, ps, controller, velocity);
      break;

    case PLAYER_STATE_CROUCH_FIRING:
      anim_state_crouch_firing(sprite_comp, ps, controller, velocity);
      break;

    case PLAYER_STATE_RELOADING:
      anim_state_reloading(sprite_comp, ps, controller, velocity);
      break;

    default:
      // Fallback to idle animation for unknown states
      if (!cf_sprite_is_playing(sprite_comp, "GunAim")) {
        cf_sprite_play(sprite_comp, "GunAim");
      }
      break;
    }

    // Update sprite animation every frame
    cf_sprite_update(sprite_comp);

    // Set horizontal flip based on facing direction
    if (controller->facing_direction.x >= 0.0f) {
      sprite_comp->scale.x = 1.0f;
    } else {
      sprite_comp->scale.x = -1.0f;
    }
  }

  return 0;
}
// NOLINTEND

// =============================================================================
// System: Render Sprites
// =============================================================================
// Draws all entities with Sprite and Transform components.

static ecs_ret_t sys_render_sprites([[maybe_unused]] ecs_t* ecs,
                                    ecs_entity_t* entities, size_t count,
                                    [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto sprite_comp = ECS_GET(entities[i], C_Sprite);
    auto transform   = ECS_GET(entities[i], C_Transform);

    cf_draw_push();
    cf_draw_translate(transform->position.x, transform->position.y);
    cf_draw_sprite(sprite_comp);
    cf_draw_pop();
  }
  return 0;
}

// =============================================================================
// Player Factory
// =============================================================================

void make_player(void) {
  // Create player entity and store in world
  ecs_entity_t player = ecs_create(state->world.ecs);
  state->world.player = player;

  // Add components to player
  ECS_ADD(player, C_PlayerInput);

  // Initialize player controller with default speeds
  auto controller              = ECS_ADD(player, C_PlayerController);
  controller->walk_speed       = 150.0f;
  controller->facing_direction = cf_v2(1.0f, 0.0f); // Default: facing right

  // Initialize player state
  auto ps              = ECS_ADD(player, C_PlayerState);
  ps->current          = PLAYER_STATE_IDLE;
  ps->previous         = PLAYER_STATE_IDLE;
  ps->state_timer      = 0.0f;
  ps->coroutine_state   = 0; // Start coroutine from beginning

  // Initialize transform at center of screen (CF origin is at center)
  auto transform      = ECS_ADD(player, C_Transform);
  transform->position = cf_v2(0.0f, 0.0f);
  transform->rotation = 0.0f;

  // Initialize velocity (stationary)
  auto velocity = ECS_ADD(player, C_Velocity);
  *velocity     = cf_v2(0.0f, 0.0f);

  // Initialize sprite with player_combat.ase (gun animations)
  auto sprite = ECS_ADD(player, C_Sprite);
  *sprite     = cf_make_sprite("assets/sprites/player_combat.ase");

  // Start with walk animation
  cf_sprite_play(sprite, "GunWalk");
}

// =============================================================================
// World Initialization
// =============================================================================

// NOLINTBEGIN
void init_world(void) {
  // Create ECS context
  state->world.ecs        = ecs_new(ECS_ENTITY_COUNT, nullptr);
  state->world.components = nullptr;
  state->world.systems    = nullptr;
  state->world.dt         = 0.0f;

  // Register components
  ECS_REGISTER_COMP(C_PlayerInput);
  ECS_REGISTER_COMP(C_PlayerController);
  ECS_REGISTER_COMP(C_PlayerState);
  ECS_REGISTER_COMP(C_Transform);
  ECS_REGISTER_COMP(C_Velocity);
  ECS_REGISTER_COMP(C_Sprite);

  // Register systems
  ECS_REGISTER_SYSTEM(sys_gather_input, nullptr);
  ECS_REQUIRE_COMP(sys_gather_input, C_PlayerInput);

  ECS_REGISTER_SYSTEM(sys_update_player_state, nullptr);
  ECS_REQUIRE_COMP(sys_update_player_state, C_PlayerState);
  ECS_REQUIRE_COMP(sys_update_player_state, C_PlayerInput);

  ECS_REGISTER_SYSTEM(sys_update_player_movement, nullptr);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_Velocity);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerController);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerState);
  ECS_REQUIRE_COMP(sys_update_player_movement, C_PlayerInput);

  ECS_REGISTER_SYSTEM(sys_apply_velocity, nullptr);
  ECS_REQUIRE_COMP(sys_apply_velocity, C_Transform);
  ECS_REQUIRE_COMP(sys_apply_velocity, C_Velocity);

  ECS_REGISTER_SYSTEM(sys_update_animation, nullptr);
  ECS_REQUIRE_COMP(sys_update_animation, C_Sprite);
  ECS_REQUIRE_COMP(sys_update_animation, C_PlayerState);
  ECS_REQUIRE_COMP(sys_update_animation, C_PlayerController);
  ECS_REQUIRE_COMP(sys_update_animation, C_Velocity);

  ECS_REGISTER_SYSTEM(sys_render_sprites, nullptr);
  ECS_REQUIRE_COMP(sys_render_sprites, C_Sprite);
  ECS_REQUIRE_COMP(sys_render_sprites, C_Transform);

  // Create player using factory function
  make_player();
}
// NOLINTEND

// =============================================================================
// Main Update Function
// =============================================================================
// Runs systems in explicit order for control over execution pipeline.

void update_world(float dt) {
  state->world.dt = dt;

  // Input
  ECS_RUN_SYSTEM(sys_gather_input);

  // Logic
  ECS_RUN_SYSTEM(sys_update_player_state);
  ECS_RUN_SYSTEM(sys_update_player_movement);

  // Physics
  ECS_RUN_SYSTEM(sys_apply_velocity);

  // Animation
  ECS_RUN_SYSTEM(sys_update_animation);
}

// =============================================================================
// Render World
// =============================================================================
// Renders all sprites using the render system.

void render_world(void) { ECS_RUN_SYSTEM(sys_render_sprites); }

// =============================================================================
// Hot Reload
// =============================================================================
// Updates system callbacks after the game library is reloaded.
// Function pointers become stale when the library is unloaded/reloaded.

void world_hot_reload(void) {
  ECS_UPDATE_SYSTEM(sys_gather_input, nullptr);
  ECS_UPDATE_SYSTEM(sys_update_player_state, nullptr);
  ECS_UPDATE_SYSTEM(sys_update_player_movement, nullptr);
  ECS_UPDATE_SYSTEM(sys_apply_velocity, nullptr);
  ECS_UPDATE_SYSTEM(sys_update_animation, nullptr);
  ECS_UPDATE_SYSTEM(sys_render_sprites, nullptr);
}

// =============================================================================
// Shutdown
// =============================================================================

void shutdown_world(void) {
  if (state->world.ecs) {
    ecs_free(state->world.ecs);
    state->world.ecs = nullptr;
  }
  if (state->world.components) {
    hfree(state->world.components);
  }
  if (state->world.systems) {
    hfree(state->world.systems);
  }
}
