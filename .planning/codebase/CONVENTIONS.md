# Coding Conventions

**Analysis Date:** 2026-03-18

## Naming Patterns

**Files:**
- `snake_case.c` and `snake_case.h` for all source and header files
- Examples: `player_system.c`, `world.h`, `game_state.h`, `input_system.c`
- System files grouped in `src/game/systems/` directory with prefix `*_system.c`

**Functions:**
- `snake_case` for public and static functions
- System functions: `sys_<name>()` pattern (e.g., `sys_gather_input()`, `sys_apply_velocity()`)
- Initialization functions: `init_<name>()` (e.g., `init_world()`)
- Factory functions: `make_<type>_at()` (e.g., `make_player_at()`)
- Getter functions: `<type>_get_<property>()` (e.g., `ldtk_entity_get_field()`)
- Helper/calculation functions: `<name>()` (e.g., `calculate_dest_size()`)
- Lifecycle functions: `<name>_hot_reload()`, `<name>_shutdown()`
- Macros at module level: `<NAME>()` in uppercase (e.g., `MAX_ENTITIES`, `ENTITY_NONE`)

**Variables:**
- Local variables: `snake_case` (e.g., `player_sprite`, `entity_aabb`)
- Struct members: `snake_case` (e.g., `facing_direction`, `collider.half_size`)
- Global pointers to state: `state` (GameState instance at `src/engine/game_state.h`)
- Tuple/iterator variables: Short names acceptable in loops (e.g., `i`, `e`, `co`)
- Temporary/calculation variables: Abbreviated when obvious from context (e.g., `lw`, `lh` for level_width/height)

**Types:**
- `PascalCase` for typedef structs and enums
- Examples: `Entity`, `Transform`, `PlayerInput`, `PlayerState`, `Collider`, `LdtkMap`, `GameState`
- Enum values: `ENUM_NAME_VALUE` in uppercase with `_` separators
- Examples: `PLAYER_STATE_IDLE`, `PLAYER_STATE_WALKING`, `LOG_LEVEL_DEBUG`

## Code Style

**Formatting:**
- Tool: `clang-format`
- Config file: `.clang-format` (LLVM-based)
- Indentation: 2 spaces (never tabs for C code)
- Tab width: 2
- Line length: 80 characters (enforced in `.editorconfig`)
- Pointer alignment: Left (`CF_V2*` not `CF_V2 *`)
- Include sorting: Case-sensitive, grouped by standard library → vendor → local

**Run formatting:**
```bash
rake format              # Format all C files with clang-format
```

**Editor config:**
- File: `.editorconfig`
- Charset: UTF-8
- End of line: LF
- Insert final newline: Yes
- Trim trailing whitespace: Yes

**Linting:**
- Compiler flags: `-Wall -Wextra -Wpedantic -Wconversion -Wdouble-promotion -Wstrict-prototypes -Wmissing-prototypes`
- Set in: `CMakeLists.txt` lines 30-35
- Additional analyzer on Debug+GNU: `-fanalyzer`
- Treat warnings as errors: Not enforced (warnings permitted)

## Import Organization

**Order:**
1. Standard C library headers in `<angle.h>` (e.g., `<stdint.h>`, `<stdbool.h>`)
2. Third-party vendor headers in `<angle.h>` (e.g., `<cute.h>`, `<SDL3/SDL_log.h>`, `<dcimgui.h>`)
3. Project headers in `"quotes"` (e.g., `"world.h"`, `"../engine/game_state.h"`)
4. Platform-specific headers last within their category

**Examples:**

From `src/game/world.c`:
```c
#include "world.h"

#include <cute_coroutine.h>
#include <cute_math.h>
#include <cute_result.h>
#include <cute_sprite.h>
#include <string.h>

#include "../engine/game_state.h"
#include "../engine/log.h"
#include "ldtk.h"
#include "rain.h"
#include "systems/systems.h"
```

From `src/game/systems/physics_system.c`:
```c
#include <cute_math.h>
#include <math.h>

#include "../../engine/game_state.h"
#include "../ldtk.h"
#include "../world.h"
#include "systems.h"
```

**Path style:**
- Use relative paths for local imports (e.g., `"../engine/log.h"`)
- Preserve order even with path nesting

## Error Handling

**Patterns:**

**Null checks (early return):**
```c
// Check for nullptr before dereferencing
if (!map->loaded) {
  return;
}
if (!level->int_grid) {
  return;
}
```

**CF_Result for external library calls:**
```c
CF_Result result = {0};
px->sky = cf_make_easy_sprite_from_png(path, &result);
if (cf_is_error(result)) {
  log_warn("world", "Failed to load parallax sky");
}
```

**Boolean state checks (filtering):**
```c
// Filter entities by existence and component enabled flags
if (!e->exists) {
  continue;
}
if (!e->transform.enabled) {
  continue;
}
if (!e->velocity.enabled) {
  continue;
}
```

**Guard clauses in loops:**
- Use `continue` to skip entities that don't match filter criteria
- Never nest deep conditionals; prefer linear early exits

**Boundary checks:**
```c
if (id >= 0 && id < MAX_ENTITIES) {
  // valid
}
```

**No exceptions or longjmp:** Pure C, no error propagation chains. Failures logged and handled locally.

## Logging

**Framework:** `log_*()` macros backed by SDL3 logging

**Macros:**
- `log_debug(tag, fmt, ...)` — DEBUG level, file/line captured
- `log_info(tag, fmt, ...)` — INFO level
- `log_warn(tag, fmt, ...)` — WARN level
- `log_error(tag, fmt, ...)` — ERROR level
- `log_fatal(tag, fmt, ...)` — CRITICAL level

**Usage pattern:**
```c
#include "../engine/log.h"

// Logging with tag and format string
log_info("world", "Player created at (%.1f, %.1f)", x, y);
log_warn("ldtk", "Failed to load map from %s", path);
log_error("physics", "Collision resolution failed for entity %d", id);
```

**Implementation:** `src/engine/log.c`
- Maps to SDL3 `SDL_LogMessage()` with auto file/line capture
- Tag is optional (can be nullptr)
- File prefix stripped via `LOG_SOURCE_DIR` macro (set in CMake)
- All logs include `[tag] file.c:line: message` format

**When to log:**
- Errors and failures: Always
- Warnings: State mismatches, missing assets, correctable problems
- Info: Initialization, loading completion
- Debug: Per-frame state changes (only in development builds)

## Comments

**When to comment:**
- Before complex algorithms or non-obvious math (e.g., coordinate transforms in `physics_system.c`)
- Section dividers: Use `// =============================================================================` for logical grouping
- Explain the "why", not the "what" (code explains itself)

**Example:**
```c
// Coordinate helpers: CF world coords <-> LDtk grid coords
// CF uses Y-up from center; LDtk uses Y-down from top-left
static int cf_x_to_grid(float cf_x, int level_width) {
  float ldtk_x = cf_x + (float)level_width / 2.0f;
  return (int)floorf(ldtk_x / (float)LDTK_GRID_SIZE);
}
```

**No JSDoc/TSDoc:** C project uses simple comments. Inline documentation at top of `.c` files (3-4 lines):
```c
// world.c - World management with fat struct entities
//
// Entity lifecycle, player factory, and system dispatch.
```

**Section markers:**
```c
// =============================================================================
// Entity Management
// =============================================================================
```

## Function Design

**Size:**
- Average 20-50 lines per function
- System functions typically 10-40 lines (filter entity loop + logic)
- Avoid nesting more than 2-3 levels; prefer early returns

**Parameters:**
- Passed by value for small types (int, float, CF_V2)
- Passed by pointer for large structs or when mutation needed
- No `const` pointers (not enforced convention)

**Return values:**
- `void` for systems and lifecycle functions
- `int` for entity IDs (returns `ENTITY_NONE` on failure)
- `bool` for success/failure checks (e.g., `ldtk_load()`, `ensure_composite_canvas()`)
- `CF_Result` for Cute Framework calls
- Void for factory functions (e.g., `make_player_at()`)

**Example function:**
```c
void sys_gather_input(void) {
  for (int i = 0; i < MAX_ENTITIES; i++) {
    Entity* e = &state->world.entities[i];
    if (!e->exists || !e->player_input.enabled) {
      continue;
    }

    e->player_input.up    = cf_key_down(CF_KEY_W) || cf_key_down(CF_KEY_UP);
    e->player_input.left  = cf_key_down(CF_KEY_A) || cf_key_down(CF_KEY_LEFT);
    // ...
  }
}
```

## Module Design

**Exports:**
- Headers (`.h`) declare public functions and types
- Implementation (`.c`) includes private helpers with `static`
- No file-scope structs unless truly private (structs usually in headers)

**Barrel files:**
- `src/game/systems/systems.h` — centralizes all system function declarations
- Used by `game.c` for dispatch loop

**Example module structure:**

`world.h`:
```c
#pragma once
// Types and constants
typedef struct Entity { ... } Entity;
#define MAX_ENTITIES 4096
// Public functions
int world_add_entity(Entity e);
void make_player_at(float x, float y);
```

`world.c`:
```c
#include "world.h"
// Static helpers
static void update_muzzle_flash(float dt) { ... }
// Public implementations
int world_add_entity(Entity e) { ... }
void make_player_at(float x, float y) { ... }
```

## Special Conventions

**Null pointer:**
- Use `nullptr` (C23 keyword), never `NULL` or `0`
- Example: `GameState* state = nullptr;`

**Unused parameters:**
- Use `[[maybe_unused]]` attribute, never `(void)parameter`
- Example: `static void on_shader_changed([[maybe_unused]] const char* path, [[maybe_unused]] void* udata)`

**Comments style:**
- Use `//` for all comments (single-line and multi-line)
- Never use `/* */` style

**Typedef naming:**
- All typedef structs are PascalCase: `typedef struct Entity { ... } Entity;`
- All typedef enums are PascalCase: `typedef enum PlayerState { ... } PlayerState;`

**Struct member defaults:**
- Initialize struct literals with designated initializers: `.field = value`
- In `make_player_at()`, full entity created with designated init and inline .enabled flags

---

*Convention analysis: 2026-03-18*
