// systems.h - System function declarations
//
// Systems iterate world entities directly, filtering by component enabled flags.

#pragma once

// Input system - reads keyboard/mouse state into PlayerInput
void sys_gather_input(void);

// Player systems - coroutine-based state and movement
void sys_player_coroutine(void);
void sys_update_player_movement(void);

// Physics system - Euler integration
void sys_apply_velocity(void);

// Render system - draws sprites at transform positions
void sys_render_sprites(void);
