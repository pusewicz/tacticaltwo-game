// systems.h - System function declarations
//
// Systems iterate world entities directly, filtering by component enabled
// flags.

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

// Collision system - resolves entity-tilemap overlaps
void sys_collide_tilemap(void);

// Debug render system - draws collider AABBs and nearby solid tiles
void sys_debug_colliders(void);

// Debug layer overlay - grid lines, IntGrid cells, entity spawn points, level bounds
void sys_debug_layers(void);

// Camera system - smooth follow with level bounds clamping
void sys_camera_follow(void);

// Parallax system - tiling background layers with depth-based scrolling
void sys_render_parallax(void);

// Tilemap render system - draws LDtk pre-rendered layer PNGs
void sys_render_tilemap(void);

// Rain audio system - procedural white noise with low-pass filter
void sys_rain_init(void);
void sys_rain_shutdown(void);
