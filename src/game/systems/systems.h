// systems.h - Forward declarations for ECS system functions
//
// All system functions follow the pico_ecs signature:
// ecs_ret_t system(ecs_t* ecs, ecs_entity_t* entities, size_t count, void*
// udata)

#pragma once

#include <stddef.h>

#include "../world.h"

// Input system - reads keyboard/mouse state into C_PlayerInput
ecs_ret_t sys_gather_input(ecs_t* ecs, ecs_entity_t* entities, size_t count,
                           void* udata);

// Player systems - state machine and movement
ecs_ret_t sys_update_player_state(ecs_t* ecs, ecs_entity_t* entities,
                                  size_t count, void* udata);
ecs_ret_t sys_update_player_movement(ecs_t* ecs, ecs_entity_t* entities,
                                     size_t count, void* udata);

// Physics system - Euler integration
ecs_ret_t sys_apply_velocity(ecs_t* ecs, ecs_entity_t* entities, size_t count,
                             void* udata);

// Animation system - state-to-animation mapping and sprite updates
ecs_ret_t sys_update_animation(ecs_t* ecs, ecs_entity_t* entities, size_t count,
                               void* udata);

// Render system - draws sprites at transform positions
ecs_ret_t sys_render_sprites(ecs_t* ecs, ecs_entity_t* entities, size_t count,
                             void* udata);
