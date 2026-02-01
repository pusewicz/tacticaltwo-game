# Animation State Machine Refactoring

## Overview

This document explains the refactoring of the animation system in `world.c` to use a switch-based state machine pattern, as inspired by [Slembcke's blog post on State Machines](https://www.slembcke.net/blog/StateMachines/).

## Problem

The original animation system had scattered logic with multiple if-statements checking for various states:

```c
// Old approach - scattered if-statements
const char* anim_name = state_to_animation(ps->current);

if (ps->current == PLAYER_STATE_FIRING) {
  if (cf_sprite_is_playing(...)) {
    anim_name = nullptr;
  } else {
    // logic
  }
}

if (ps->current == PLAYER_STATE_CROUCH_FIRING) {
  // more logic
}

// Later in the function...
if (ps->state_timer > 0.0f && (ps->current == PLAYER_STATE_RELOADING || ...)) {
  // transition logic
}
```

This approach had several issues:
- **Scattered logic**: State-specific behavior was spread across multiple if-statements
- **Hard to follow**: State transitions weren't clearly visible
- **Difficult to maintain**: Adding new states required modifying multiple locations

## Solution: Switch-Based State Machine

The refactored code uses a switch statement where each case handles a specific state's:
1. **Entry behavior** - What animation to play when entering the state
2. **Update behavior** - Per-frame logic for that state
3. **Exit behavior** - Conditions and transitions to other states

```c
switch (ps->current) {
case PLAYER_STATE_IDLE:
  // Entry: play aim animation
  if (!cf_sprite_is_playing(sprite_comp, "GunAim")) {
    cf_sprite_play(sprite_comp, "GunAim");
  }
  break;

case PLAYER_STATE_FIRING:
  // Entry: select appropriate fire animation
  if (!cf_sprite_is_playing(sprite_comp, "GunFire") &&
      !cf_sprite_is_playing(sprite_comp, "GunWalkFire")) {
    bool moving       = velocity->x != 0.0f;
    const char* anim  = moving ? "GunWalkFire" : "GunFire";
    cf_sprite_play(sprite_comp, anim);
  }

  // Exit: transition when animation completes
  if (ps->state_timer > 0.0f) {
    bool should_finish = /* ... */;
    if (should_finish) {
      ps->current = PLAYER_STATE_IDLE;
    }
  }
  break;

// ... other states
}
```

## Benefits

### 1. **Clear State Encapsulation**
Each state's logic is contained within its own case block. You can see at a glance what happens in each state without scanning the entire function.

### 2. **Explicit Transitions**
State transitions are now clearly visible within each state case. For example:
- `PLAYER_STATE_FIRING` → `PLAYER_STATE_IDLE` when animation completes
- `PLAYER_STATE_CROUCH_FIRING` → `PLAYER_STATE_CROUCHING` when animation completes
- `PLAYER_STATE_RELOADING` → `PLAYER_STATE_IDLE` when animation completes

### 3. **Easy to Extend**
Adding a new state is straightforward:
1. Add a new case to the switch statement
2. Implement entry, update, and exit logic within that case
3. Done!

### 4. **Better Locality**
All logic for a specific state is in one place. This makes debugging easier and reduces the chance of forgetting to update related code when modifying a state.

## State Descriptions

### Simple States (No Transitions)
- **IDLE**: Plays "GunAim" animation, loops indefinitely
- **WALKING**: Plays "GunWalk" animation, loops indefinitely
- **CROUCHING/CROUCH_WALKING**: Plays "GunCrouch" animation, loops indefinitely

### Complex States (With Transitions)
- **FIRING**: 
  - Entry: Selects "GunFire" or "GunWalkFire" based on velocity
  - Exit: Transitions to IDLE when animation completes
  - Special: "GunWalkFire" only plays 4 of 8 frames

- **CROUCH_FIRING**:
  - Entry: Plays "GunCrouchFire" animation
  - Exit: Transitions to CROUCHING when animation completes

- **RELOADING**:
  - Entry: Plays "GunReload" animation
  - Exit: Transitions to IDLE when animation completes

## Changes Made

### Removed
- `state_to_animation()` helper function - no longer needed as animation selection is per-state

### Modified
- `sys_update_animation()` - completely refactored to use switch-based state machine

### Added
- Clear per-state logic within switch cases
- Inline comments explaining entry/exit behavior for each state
- Default case for unknown states (safety fallback)

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

## Future Improvements

The switch-based state machine provides a solid foundation for future enhancements:
- Add new animation states easily
- Implement state entry/exit callbacks if needed
- Add state-specific timers or counters
- Support more complex state transitions

## References

- [State Machines in C - Slembcke.net](https://www.slembcke.net/blog/StateMachines/)
- Original implementation: `world.c` (before refactoring)
