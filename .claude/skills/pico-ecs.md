---
description: Use when working with ECS (Entity Component System), creating entities, components, systems, or game architecture using pico_ecs
---

# pico_ecs - Entity Component System

A pure C99 single-header ECS library. Include from `include/pico_ecs.h`.

## Setup Pattern (toy_tunnel style)

Create a world header with implementation and convenience macros:

```c
// world.h
#pragma once

#define PICO_ECS_MAX_COMPONENTS 32
#define PICO_ECS_MAX_SYSTEMS 64
#define PICO_ECS_IMPLEMENTATION
#include "pico_ecs.h"

#ifndef ECS_ENTITY_COUNT
#define ECS_ENTITY_COUNT 4096
#endif

// Convenience macros using Cute Framework hashtables
#define ECS_GET_COMP(COMP) cf_hashtable_get(state->world.components, cf_sintern(#COMP))
#define ECS_GET_SYSTEM(SYSTEM) cf_hashtable_get(state->world.systems, cf_sintern(#SYSTEM))

#define ECS_REGISTER_COMP(COMP) \
{ \
    ecs_comp_t id = ecs_define_component(state->world.ecs, sizeof(COMP), NULL, NULL); \
    cf_hashtable_set(state->world.components, cf_sintern(#COMP), id); \
}

#define ECS_REGISTER_COMP_CB(COMP, CTOR, DTOR) \
{ \
    ecs_comp_t id = ecs_define_component(state->world.ecs, sizeof(COMP), CTOR, DTOR); \
    cf_hashtable_set(state->world.components, cf_sintern(#COMP), id); \
}

#define ECS_REGISTER_SYSTEM(SYSTEM) \
{ \
    ecs_system_t id = ecs_define_system(state->world.ecs, (ecs_mask_t){0}, SYSTEM, NULL, NULL, NULL); \
    cf_hashtable_set(state->world.systems, cf_sintern(#SYSTEM), id); \
}

#define ECS_REQUIRE_COMP(SYSTEM, COMP) \
    ecs_require_component(state->world.ecs, ECS_GET_SYSTEM(SYSTEM), ECS_GET_COMP(COMP))

#define ECS_EXCLUDE_COMP(SYSTEM, COMP) \
    ecs_exclude_component(state->world.ecs, ECS_GET_SYSTEM(SYSTEM), ECS_GET_COMP(COMP))

#define ECS_GET(ENTITY, COMP) ecs_get(state->world.ecs, ENTITY, ECS_GET_COMP(COMP))

// Component structs (prefix with C_)
typedef struct C_Transform {
    float x, y;
    float rotation;
} C_Transform;

typedef struct C_Movement {
    float vx, vy;
} C_Movement;

typedef struct C_Sprite {
    CF_Sprite sprite;
} C_Sprite;

typedef struct C_Player {
    int health;
} C_Player;

typedef struct C_Enemy {
    int type;
} C_Enemy;

// World state (add to GameState struct)
typedef struct World {
    ecs_t* ecs;
    CF_Htbl ecs_system_t* systems;
    CF_Htbl ecs_comp_t* components;
    float dt;
} World;

// Lifecycle
void init_world(void);
void update_world(void);
void draw_world(void);
void clear_world(void);

// Entity factories (prefix with make_)
ecs_entity_t make_player(float x, float y);
ecs_entity_t make_enemy(float x, float y, int type);

// System callbacks (prefix with system_)
ecs_ret_t system_update_movement(ecs_t* ecs, ecs_entity_t* entities, size_t count, void* udata);
ecs_ret_t system_update_players(ecs_t* ecs, ecs_entity_t* entities, size_t count, void* udata);
ecs_ret_t system_draw_sprites(ecs_t* ecs, ecs_entity_t* entities, size_t count, void* udata);
```

## GameState Integration

Add World to existing GameState:

```c
// game_state.h
#pragma once

#include "world.h"

typedef struct GameState {
    Platform* platform;
    CF_Arena* scratch_arena;
    bool debug_mode;

    World world;  // ECS world state
} GameState;

extern GameState* state;
```

## ECS Initialization

Register components first, then systems with their requirements:

```c
// world.c
#include "world.h"
#include "game_state.h"

void init_ecs(void)
{
    state->world.ecs = ecs_new(ECS_ENTITY_COUNT, NULL);

    // Register components
    {
        ECS_REGISTER_COMP(C_Transform);
        ECS_REGISTER_COMP(C_Movement);
        ECS_REGISTER_COMP(C_Sprite);
        ECS_REGISTER_COMP(C_Player);
        ECS_REGISTER_COMP(C_Enemy);
    }

    // Register systems with required components
    {
        ECS_REGISTER_SYSTEM(system_update_movement);
        ECS_REQUIRE_COMP(system_update_movement, C_Transform);
        ECS_REQUIRE_COMP(system_update_movement, C_Movement);
    }

    {
        ECS_REGISTER_SYSTEM(system_update_players);
        ECS_REQUIRE_COMP(system_update_players, C_Transform);
        ECS_REQUIRE_COMP(system_update_players, C_Player);
    }

    {
        ECS_REGISTER_SYSTEM(system_draw_sprites);
        ECS_REQUIRE_COMP(system_draw_sprites, C_Transform);
        ECS_REQUIRE_COMP(system_draw_sprites, C_Sprite);
        ECS_EXCLUDE_COMP(system_draw_sprites, C_Enemy);  // Example exclusion
    }
}

void init_world(void)
{
    init_ecs();
    // Additional world initialization...
}
```

## Update & Draw Loops

Run systems explicitly in desired order:

```c
void update_world(void)
{
    ecs_t* ecs = state->world.ecs;
    state->world.dt = CF_DELTA_TIME;

    // Update systems in order
    ecs_run_system(ecs, ECS_GET_SYSTEM(system_update_movement), 0);
    ecs_run_system(ecs, ECS_GET_SYSTEM(system_update_players), 0);
}

void draw_world(void)
{
    ecs_t* ecs = state->world.ecs;

    ecs_run_system(ecs, ECS_GET_SYSTEM(system_draw_sprites), 0);
}

void clear_world(void)
{
    ecs_reset(state->world.ecs);  // Remove all entities, keep systems/components
}
```

## System Implementation

Use C23 `[[maybe_unused]]` for unused parameters:

```c
ecs_ret_t system_update_movement([[maybe_unused]] ecs_t* ecs,
                                  ecs_entity_t* entities,
                                  size_t count,
                                  [[maybe_unused]] void* udata)
{
    float dt = state->world.dt;

    for (size_t i = 0; i < count; i++) {
        C_Transform* transform = ECS_GET(entities[i], C_Transform);
        C_Movement* movement = ECS_GET(entities[i], C_Movement);

        transform->x += movement->vx * dt;
        transform->y += movement->vy * dt;
    }

    return 0;
}

ecs_ret_t system_draw_sprites([[maybe_unused]] ecs_t* ecs,
                               ecs_entity_t* entities,
                               size_t count,
                               [[maybe_unused]] void* udata)
{
    for (size_t i = 0; i < count; i++) {
        C_Transform* transform = ECS_GET(entities[i], C_Transform);
        C_Sprite* sprite = ECS_GET(entities[i], C_Sprite);

        cf_draw_sprite(&sprite->sprite);
    }

    return 0;
}
```

## Entity Factories

Create entities with consistent component configurations:

```c
ecs_entity_t make_player(float x, float y)
{
    ecs_t* ecs = state->world.ecs;
    ecs_entity_t entity = ecs_create(ecs);

    C_Transform* transform = ecs_add(ecs, entity, ECS_GET_COMP(C_Transform), NULL);
    transform->x = x;
    transform->y = y;
    transform->rotation = 0;

    C_Movement* movement = ecs_add(ecs, entity, ECS_GET_COMP(C_Movement), NULL);
    movement->vx = 0;
    movement->vy = 0;

    ecs_add(ecs, entity, ECS_GET_COMP(C_Sprite), NULL);

    C_Player* player = ecs_add(ecs, entity, ECS_GET_COMP(C_Player), NULL);
    player->health = 100;

    return entity;
}

ecs_entity_t make_enemy(float x, float y, int type)
{
    ecs_t* ecs = state->world.ecs;
    ecs_entity_t entity = ecs_create(ecs);

    C_Transform* transform = ecs_add(ecs, entity, ECS_GET_COMP(C_Transform), NULL);
    transform->x = x;
    transform->y = y;

    ecs_add(ecs, entity, ECS_GET_COMP(C_Movement), NULL);
    ecs_add(ecs, entity, ECS_GET_COMP(C_Sprite), NULL);

    C_Enemy* enemy = ecs_add(ecs, entity, ECS_GET_COMP(C_Enemy), NULL);
    enemy->type = type;

    return entity;
}
```

## Component Destructors

Use for cleanup (coroutines, resources):

```c
void comp_weapon_destructor([[maybe_unused]] ecs_t* ecs,
                            [[maybe_unused]] ecs_entity_t entity,
                            void* comp_ptr)
{
    C_Weapon* weapon = (C_Weapon*)comp_ptr;
    if (weapon && weapon->co.id) {
        cf_destroy_coroutine(weapon->co);
    }
}

// Register with destructor:
ECS_REGISTER_COMP_CB(C_Weapon, NULL, comp_weapon_destructor);
```

## Safe Entity Destruction

Always use queued destruction inside systems:

```c
ecs_ret_t system_update_bullets([[maybe_unused]] ecs_t* ecs,
                                 ecs_entity_t* entities,
                                 size_t count,
                                 [[maybe_unused]] void* udata)
{
    for (size_t i = 0; i < count; i++) {
        C_Bullet* bullet = ECS_GET(entities[i], C_Bullet);

        if (bullet->lifetime <= 0) {
            ecs_queue_destroy(state->world.ecs, entities[i]);  // Safe: destroys after system returns
        }
    }
    return 0;
}
```

## Core API Reference

| Function | Description |
|----------|-------------|
| `ecs_new(count, ctx)` | Create ECS context |
| `ecs_free(ecs)` | Destroy ECS context |
| `ecs_reset(ecs)` | Remove all entities, keep systems |
| `ecs_create(ecs)` | Create entity |
| `ecs_add(ecs, entity, comp, args)` | Add component, returns ptr |
| `ecs_get(ecs, entity, comp)` | Get component ptr |
| `ecs_has(ecs, entity, comp)` | Check if entity has component |
| `ecs_queue_destroy(ecs, entity)` | Safe destroy after system |
| `ecs_queue_remove(ecs, entity, comp)` | Safe remove after system |
| `ecs_run_system(ecs, sys, mask)` | Run single system |
| `ecs_run_systems(ecs, mask)` | Run all systems |
| `ecs_require_component(ecs, sys, comp)` | System requires component |
| `ecs_exclude_component(ecs, sys, comp)` | System excludes component |

## Naming Conventions

| Type | Prefix | Example |
|------|--------|---------|
| Components | `C_` | `C_Transform`, `C_Player` |
| Systems | `system_` | `system_update_movement` |
| Factories | `make_` | `make_player`, `make_bullet` |
| Destructors | `comp_*_destructor` | `comp_weapon_destructor` |

## Best Practices

1. **Use the macro pattern** - `ECS_REGISTER_COMP`, `ECS_GET_COMP`, etc. for cleaner code
2. **Group system registration** with braces for readability
3. **Run systems explicitly** in `update_world()` and `draw_world()` in desired order
4. **Use `ecs_queue_destroy()`** inside systems, never `ecs_destroy()`
5. **Use destructors** for components that own resources (coroutines, allocations)
6. **Factory functions** ensure consistent entity creation
7. **Use `[[maybe_unused]]`** for unused system callback parameters
8. **Use `#pragma once`** for header include guards
