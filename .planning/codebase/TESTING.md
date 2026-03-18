# Testing Patterns

**Analysis Date:** 2026-03-18

## Test Framework

**Status:** No automated testing framework installed or configured

**Current state:**
- No test runner (Jest, Vitest, CMake CTest, etc.)
- No test files in repository
- All testing is manual during development

**Why:** This is a game developed with hot-reloading and ImGui debug UI. Testing happens through:
1. Runtime debug overlay in `src/game/game.c` (ImGui docked layout)
2. Visual inspection of game behavior
3. Manual testing via `rake run` or `rake watch`

## Manual Testing Approach

**Debug mode:**
- Toggleable via `G` key during gameplay
- Enabled in `src/game/game.c` lines 117-119
- Shows ImGui docked layout with:
  - **Game window:** Composite canvas preview maintaining aspect ratio
  - **Debug window:** Live tweaking of game parameters
  - **Collider info:** Player half-size, offset, grounded state
  - **Transform info:** Position and velocity readout
  - **Level info:** Active level name, position, grid size
  - **Rain controls:** Enable/disable, intensity, density, speed, wind, splash parameters
  - **Lighting controls:** Ambient level, HRC grid size selector

**Parameter tweaking:**
```
ImGui_SliderFloat("half_w", &player->collider.half_size.x, 1.0f, 32.0f);
ImGui_SliderFloat("offset_y", &player->collider.offset.y, -48.0f, 48.0f);
```
- Changes persist across hot reloads (values stored in GameState)
- Immediate visual feedback in Game viewport

**Hot-reload workflow:**
```bash
rake watch              # Watch src/ for changes, auto-rebuild libgame.dylib
# Edit src/game/*.c
# See changes instantly (no app restart)
# Test with debug overlay
```

## Test Data & Verification

**Assets used for testing:**
- Player sprite: `assets/sprites/player_combat.ase`
- Level/map: `assets/ldtk/map/simplified/` (LDtk simplified export)
- Background layers: `assets/GandalfHardcore City Tiles/*.png`
- Shaders: `assets/shaders/*.glsl`

**Known test scenarios (manual):**

**Player movement:**
1. Start game
2. Open debug (G key)
3. Press A/D to move left/right
4. Verify velocity changes in Debug window
5. Verify collider grounded state changes with jump

**Player state transitions:**
1. Enable debug mode
2. Move (should play "GunWalk" animation)
3. Press Ctrl+Click to crouch-fire (triggers "GunCrouchFire" animation)
4. Observe state changes in `player_state.current` display

**Collision system:**
1. Debug mode enabled
2. Watch `collider.grounded` in Debug window
3. Player should be grounded on level IntGrid tiles
4. Jump (Space) should clear grounded, then re-ground after falling

**Lighting system:**
1. Toggle `rain enabled` checkbox
2. Adjust `intensity`, `density`, `alpha`, `speed`, `wind` sliders
3. Verify visual changes in Game viewport

## Build & Verification Commands

**Development build with hot reload:**
```bash
rake watch              # Start file watcher, auto-rebuild on changes
# In separate terminal:
rake run                # Build and run game executable
```

**Standalone build:**
```bash
rake                    # Build RelWithDebInfo (default)
rake run                # Build and run
```

**Manual verification after changes:**
1. Edit source file
2. Wait for `rake watch` to rebuild (watch log shows `ninja: no work to do.` = success)
3. Game auto-reloads shared library in real-time
4. Open debug overlay (G) to inspect changed values
5. Run gameplay scenarios to verify behavior

## Code Coverage / Quality Assurance

**Static analysis:**
- Enabled in Debug + GNU: CMake adds `-fanalyzer` flag
- Compiler warnings: `-Wall -Wextra -Wpedantic -Wconversion` always enabled
- No formal coverage reporting tool

**Areas tested manually:**
- **Entity system:** Adding/removing entities, filtering by component enabled flags
- **Physics:** Velocity application, collider offset calculations, grounding detection
- **Input:** Keyboard state translation to PlayerInput component
- **Sprite animation:** State machine driving animation selection and frame progression
- **LDtk loading:** Map deserialization, level switching, entity spawning
- **Rendering:** Canvas compositing, parallax scrolling, debug overlays

**Areas NOT systematically tested:**
- Memory management (no leak detection tool)
- Edge cases in coordinate transforms (CF Y-up vs LDtk Y-down)
- Concurrent entity modifications during iteration
- Large map loading performance

## Testing Best Practices

**When adding new features:**

1. **Add ImGui debug controls for new parameters**
   - Location: `src/game/game.c` in the Debug window section
   - Example: Add `ImGui_SliderFloat("new_param", &state->value, min, max);`
   - Allows real-time tweaking without recompilation

2. **Use debug collider visualization**
   - Location: `src/game/systems/render_system.c` (not shown, but referenced)
   - Shows AABB boxes and collision grid cells
   - Verify visually if new collision logic is correct

3. **Test with hot reload**
   - Edit `src/game/*.c`
   - Run `rake watch` and observe shared lib rebuild
   - Game should reflect changes without restart
   - Check GameState persistence (values should survive reload)

4. **Verify log output**
   - Location: Console output from `log_*()` macros
   - Check for warnings: `log_warn("tag", "message")`
   - Errors should appear as: `log_error("tag", "message")`
   - File/line info included automatically

## Notable Testing Gaps

**No automated tests for:**
- Entity lifecycle (add/remove/query)
- System filtering logic (component enabled flags)
- Coordinate transformation math (CF vs LDtk)
- Animation state transitions
- Collision resolution edge cases
- Memory allocation/deallocation patterns

**Risk areas if changes made:**
- `src/game/world.c` — Entity management, could have use-after-free bugs
- `src/game/systems/physics_system.c` — Coordinate math, could break collision
- `src/game/systems/animation_system.c` — State machine, could cause animation glitches
- `src/game/ldtk.c` — JSON parsing, could fail on map format changes

## Future Testing Infrastructure

**Recommended but not implemented:**
- CMake CTest for unit tests (C++/C hybrid)
- Custom assertion macros for entity/physics tests
- Screenshot diffing for visual regression (compare Debug viewport before/after changes)
- Performance profiling with Cute Framework's built-in metrics
- Memory validation with ASAN or Valgrind during CI builds

---

*Testing analysis: 2026-03-18*
