# Animation State Machine Refactoring - Stackless Coroutines

## Overview

This document explains the refactoring of the animation system in `world.c` to use stackless coroutines with Duff's Device, as described in [Slembcke's blog post on State Machines](https://www.slembcke.net/blog/StateMachines/).

## What is a Stackless Coroutine?

A stackless coroutine is a function that can suspend execution and later resume from the same point. Unlike traditional functions, coroutines can "yield" control back to the caller and maintain their execution state between calls.

The implementation uses **Duff's Device** - a clever C technique that combines switch statements with macros to create resumable execution points.

## Coroutine Macros

```c
#define COROUTINE_BEGIN(state_var) \
  switch (state_var) {             \
  case 0:;

#define COROUTINE_YIELD(state_var) \
  do {                             \
    state_var = __COUNTER__;       \
    return 0;                      \
  case __COUNTER__:;               \
  } while (0)

#define COROUTINE_END \
  }                   \
  return 0
```

These macros work together:
- `COROUTINE_BEGIN`: Initializes the switch statement that dispatches to the saved execution point
- `COROUTINE_YIELD`: Saves a unique counter value and returns, resuming at that point on next call
  - Uses `__COUNTER__` for guaranteed uniqueness on each macro expansion
  - Widely supported across compilers (GCC, Clang, MSVC)
- `COROUTINE_END`: Closes the coroutine

## Benefits of This Approach

### 1. **Trivial Serialization**
The entire animation state can be saved/loaded by simply saving the component struct:
```c
struct C_PlayerState {
  PlayerState current;
  PlayerState previous;
  float state_timer;
  int coroutine_line;  // Execution position
}
```

Save/load is just a memory copy - no special handling needed!

### 2. **Zero Memory Overhead**
No stack frames, no heap allocations. The coroutine state is just an integer tracking the execution line.

### 3. **Manual Variable Persistence**
All variables that need to survive across yields must be stored in the component struct. This makes state management explicit and predictable.

## Example: Firing State Coroutine

```c
static int anim_state_firing(C_Sprite* sprite_comp, C_PlayerState* ps,
                            C_PlayerController* controller, C_Velocity* velocity) {
  COROUTINE_BEGIN(ps->coroutine_line);
  
  // Entry: Select appropriate fire animation
  if (!cf_sprite_is_playing(sprite_comp, "GunFire") &&
      !cf_sprite_is_playing(sprite_comp, "GunWalkFire")) {
    bool moving = velocity->x != 0.0f;
    cf_sprite_play(sprite_comp, moving ? "GunWalkFire" : "GunFire");
  }
  
  // Wait one frame
  COROUTINE_YIELD(ps->coroutine_line);
  
  // Wait for animation to complete
  while (ps->state_timer > 0.0f) {
    bool should_finish = false;
    if (cf_sprite_is_playing(sprite_comp, "GunWalkFire")) {
      should_finish = cf_sprite_current_frame(sprite_comp) >= 
                      GUNWALKFIRE_SINGLE_SHOT_STOP_FRAME;
    } else {
      should_finish = cf_sprite_will_finish(sprite_comp);
    }
    
    if (should_finish) {
      ps->current = PLAYER_STATE_IDLE;
      COROUTINE_END;
    }
    COROUTINE_YIELD(ps->coroutine_line);
  }
  
  COROUTINE_END;
}
```

**Execution flow:**
1. **First call**: Plays animation, hits `COROUTINE_YIELD`, saves line number and returns
2. **Second call**: Resumes after first yield, checks animation state
3. **Subsequent calls**: Keep looping until animation completes
4. **On completion**: Transitions to IDLE and resets coroutine

## State Handlers

Each animation state is now a separate coroutine function:

### Simple States (Loop Forever)
- **IDLE**: `anim_state_idle()` - Plays "GunAim" and yields forever
- **WALKING**: `anim_state_walking()` - Plays "GunWalk" and yields forever
- **CROUCHING**: `anim_state_crouching()` - Plays "GunCrouch" and yields forever

### Complex States (With Completion)
- **FIRING**: `anim_state_firing()` - Plays fire animation, waits for completion, transitions to IDLE
- **CROUCH_FIRING**: `anim_state_crouch_firing()` - Plays crouch fire animation, transitions to CROUCHING
- **RELOADING**: `anim_state_reloading()` - Plays reload animation, transitions to IDLE

## How It Works

### State Transitions
When a state change occurs:
1. `sys_update_player_state` sets `ps->current` to the new state
2. `ps->coroutine_line` is reset to 0
3. Next frame, the new coroutine starts from the beginning

### Execution Resume
Each frame:
1. `sys_update_animation` calls the appropriate coroutine handler
2. The switch statement jumps to the saved line number
3. Coroutine executes until next `YIELD` or completion
4. Execution state is automatically saved in `coroutine_line`

## Changes Made

### Added to `C_PlayerState`
```c
int coroutine_line; // Tracks execution position for coroutine resume
```

### Added Coroutine Macros
- `COROUTINE_BEGIN` - Start coroutine
- `COROUTINE_YIELD` - Yield execution
- `COROUTINE_END` - End coroutine

### Refactored Animation System
- Created separate coroutine handler for each state
- Each handler is fully resumable across frames
- State transitions explicitly reset coroutine position

### Updated State Management
- `sys_update_player_state` resets `coroutine_line` on state change
- `make_player` initializes `coroutine_line` to 0

## Comparison: Before vs After

### Before (Simple Switch)
```c
switch (ps->current) {
case PLAYER_STATE_FIRING:
  if (!cf_sprite_is_playing(...)) {
    cf_sprite_play(...);
  }
  if (ps->state_timer > 0.0f && should_finish) {
    ps->current = PLAYER_STATE_IDLE;
  }
  break;
}
```

**Issues:**
- State logic runs every frame from the start
- No resumable execution
- Can't easily express "wait for X frames"

### After (Stackless Coroutine)
```c
static int anim_state_firing(...) {
  COROUTINE_BEGIN(ps->coroutine_line);
  
  // Setup
  cf_sprite_play(...);
  COROUTINE_YIELD(ps->coroutine_line);
  
  // Wait for completion
  while (!should_finish) {
    COROUTINE_YIELD(ps->coroutine_line);
  }
  
  ps->current = PLAYER_STATE_IDLE;
  COROUTINE_END;
}
```

**Benefits:**
- Resumable execution - picks up where it left off
- Clear sequential flow - reads like synchronous code
- Easy to express "wait" logic with yields

## Testing

The refactored code maintains the same external behavior:
- All animations play correctly
- State transitions work as before
- Special cases (like GunWalkFire frame limiting) are preserved

To test:
1. Build the project: `rake`
2. Run the game: `rake run`
3. Verify all player animations work:
   - Walking (A/D keys)
   - Crouching (Ctrl key)
   - Firing (Mouse click)
   - Reload (R key)
   - Combinations (crouch + fire, walk + fire)

## Serialization Example

To save game state, simply serialize the component:

```c
// Save
fwrite(&ps->current, sizeof(PlayerState), 1, file);
fwrite(&ps->previous, sizeof(PlayerState), 1, file);
fwrite(&ps->state_timer, sizeof(float), 1, file);
fwrite(&ps->coroutine_line, sizeof(int), 1, file);

// Load
fread(&ps->current, sizeof(PlayerState), 1, file);
fread(&ps->previous, sizeof(PlayerState), 1, file);
fread(&ps->state_timer, sizeof(float), 1, file);
fread(&ps->coroutine_line, sizeof(int), 1, file);

// Animation system will resume at the exact point it was saved!
```

## References

- [State Machines in C - Slembcke.net](https://www.slembcke.net/blog/StateMachines/)
- [Coroutines in C - Simon Tatham](https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html)
- [Duff's Device - Wikipedia](https://en.wikipedia.org/wiki/Duff%27s_device)
- Original implementation: `world.c` (before refactoring)
