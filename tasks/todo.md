# Particle System Design (Compute Shaders + HRC Integration)

## Design Status
- [x] Research article technique (Turitzin's atomicAdd rendering)
- [x] Audit codebase: rendering pipeline, HRC, CF compute API, hot-reload
- [x] Write design document
- [ ] User review & approval
- [ ] Implementation

---

## Overview

A GPU-driven particle system using compute shaders for both **simulation** and
**rendering**, inspired by [Mike Turitzin's technique](https://miketuritzin.com/post/rendering-particles-with-compute-shaders/).
Particles optionally emit light into the HRC global illumination pipeline.

### Why Compute for This Game?

- **Simulation**: thousands of particles updated in parallel — no CPU round-trip
- **Rendering**: Turitzin's atomicAdd approach writes single-pixel particles
  directly into a buffer, avoiding vertex submission and overdraw overhead.
  At 480×270 this is extremely cheap
- **HRC integration**: the simulation compute pass can simultaneously scatter
  emissive particles into the emissivity canvas, getting free GI interaction
  with zero extra draw calls

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     CPU (per frame)                      │
│                                                          │
│  1. Update emitters (spawn params → uniform buffer)      │
│  2. Kick compute dispatches:                             │
│     a. Spawn shader  (fill dead slots with new particles)│
│     b. Update shader (integrate physics, age, kill)      │
│     c. Render shader (atomicAdd into pixel buffer)       │
│     d. Emit shader   (scatter light into emissivity)     │
│  3. Blit pixel buffer onto game canvas                   │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────── GPU ──────────────────────────┐
│                                                          │
│  Storage Buffers:                                        │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Particles   │  │  Dead Index  │  │  Alive Index   │  │
│  │  (SoA SSBO)  │  │  Stack       │  │  List          │  │
│  └──────────────┘  └──────────────┘  └───────────────┘  │
│                                                          │
│  ┌──────────────┐  ┌──────────────┐                      │
│  │  Counters    │  │  Pixel Buffer│  (480×270 uvec2)     │
│  │  (atomic)    │  │  (atomicAdd) │                      │
│  └──────────────┘  └──────────────┘                      │
│                                                          │
│  Textures (rw):                                          │
│  ┌──────────────┐                                        │
│  │  Emissivity  │  (shared with HRC — write emissive     │
│  │  Canvas      │   particles directly)                  │
│  └──────────────┘                                        │
└──────────────────────────────────────────────────────────┘
```

---

## Data Structures

### Particle (GPU-side, SoA layout in SSBOs)

Using Structure-of-Arrays for coalesced GPU memory access. Each array is a
separate storage buffer so shaders only bind what they need.

```glsl
// positions.c_shd buffer
layout(std430, set=1, binding=0) buffer Pos { vec2 data[]; } u_pos;

// velocities.c_shd buffer
layout(std430, set=1, binding=0) buffer Vel { vec2 data[]; } u_vel;

// life: x = remaining, y = initial (for normalized age)
layout(std430, set=1, binding=0) buffer Life { vec2 data[]; } u_life;

// packed color: rgba8 as uint (or f16x4 as uvec2 for HDR)
layout(std430, set=1, binding=0) buffer Color { uvec2 data[]; } u_color;

// flags: emissive intensity, size, type, etc. packed into uvec2
layout(std430, set=1, binding=0) buffer Flags { uvec2 data[]; } u_flags;
```

Total per particle: `2+2+2+2+2 = 10 floats = 40 bytes`
At 16K particles: **640 KB** GPU memory (trivial).

### Particle Pool Management (GPU-side)

```glsl
// Atomic counters for lock-free pool management
layout(std430, set=1, binding=0) buffer Counters {
    uint dead_count;    // top of dead stack
    uint alive_count;   // length of alive list
} u_counters;

// Dead index stack — indices of free particle slots
layout(std430, set=1, binding=0) buffer Dead { uint data[]; } u_dead;

// Alive index list — indices of living particles (rebuilt each frame)
layout(std430, set=1, binding=0) buffer Alive { uint data[]; } u_alive;
```

### Pixel Buffer (Turitzin Rendering)

```glsl
// 480 × 270 × 2 uints = ~1 MB
// Each pixel stores accumulated (R16G16, B16A16) via packHalf2x16
layout(std430, set=1, binding=0) buffer PixBuf { uvec2 data[]; } u_pixels;
```

### Emitter (CPU-side, passed as uniforms)

```c
typedef struct ParticleEmitter {
    float x, y;                // world position
    float spread;              // emission cone half-angle (radians)
    float direction;           // emission direction (radians)
    float speed_min, speed_max;
    float life_min, life_max;
    float r, g, b, a;         // base color
    float emissive;            // light intensity (0 = no light emission)
    float gravity;             // downward acceleration
    float drag;                // velocity damping per second
    float size;                // particle size (1 = single pixel)
    int   spawn_count;         // particles to spawn this frame
    int   type;                // behavior flags (e.g., collide with tilemap)
} ParticleEmitter;
```

### ParticleState (CPU-side, lives in World — survives hot-reload)

```c
#define PARTICLES_MAX       16384
#define PARTICLES_PIXEL_W   480   // matches CANVAS_WIDTH
#define PARTICLES_PIXEL_H   270   // matches CANVAS_HEIGHT

typedef struct ParticleState {
    // SoA storage buffers (GPU particle data)
    CF_StorageBuffer buf_pos;
    CF_StorageBuffer buf_vel;
    CF_StorageBuffer buf_life;
    CF_StorageBuffer buf_color;
    CF_StorageBuffer buf_flags;

    // Pool management
    CF_StorageBuffer buf_dead;
    CF_StorageBuffer buf_alive;
    CF_StorageBuffer buf_counters;

    // Turitzin pixel buffer (CANVAS_WIDTH × CANVAS_HEIGHT × uvec2)
    CF_StorageBuffer buf_pixels;

    // Output canvas (blitted over game canvas with additive blend)
    CF_Canvas        output;

    // Compute shaders
    CF_ComputeShader cs_spawn;
    CF_ComputeShader cs_update;
    CF_ComputeShader cs_render;
    CF_ComputeShader cs_emit_light;
    CF_ComputeShader cs_clear_pixels;
    CF_ComputeShader cs_blit;

    // Materials (one per shader pass)
    CF_Material      mat_spawn;
    CF_Material      mat_update;
    CF_Material      mat_render;
    CF_Material      mat_emit_light;
    CF_Material      mat_clear_pixels;
    CF_Material      mat_blit;

    bool initialized;
} ParticleState;
```

---

## Compute Pipeline (Per Frame)

### Pass 1: Clear Pixel Buffer
```
Shader:   particles_clear.c_shd
Dispatch: ceil(480*270 / 256), 1, 1
RW:       buf_pixels
```
Zero out the pixel accumulation buffer. Simple `u_pixels.data[id] = uvec2(0)`.

### Pass 2: Spawn
```
Shader:   particles_spawn.c_shd
Dispatch: ceil(spawn_count / 64), 1, 1
Uniforms: emitter params, time, seed
RW:       buf_pos, buf_vel, buf_life, buf_color, buf_flags,
          buf_dead, buf_counters
```
Each thread atomically decrements `dead_count`, pops a dead index, and
initializes that particle slot from emitter parameters + randomization
(hash-based RNG seeded from `gl_GlobalInvocationID + frame`).

Multiple emitters: dispatch once per emitter with different uniforms.
At typical spawn rates (10-100/frame) this is <1 workgroup per emitter.

### Pass 3: Update (Simulate)
```
Shader:   particles_update.c_shd
Dispatch: ceil(PARTICLES_MAX / 256), 1, 1
Uniforms: dt, gravity, camera_pos
RW:       buf_pos, buf_vel, buf_life, buf_flags
          buf_alive, buf_dead, buf_counters
```
Each thread processes one particle slot. If `life.x > 0`:
- Integrate: `vel += gravity * dt`, `vel *= (1 - drag * dt)`, `pos += vel * dt`
- Age: `life.x -= dt`
- If `life.x <= 0`: push index onto dead stack via `atomicAdd(dead_count, 1)`
- If alive: append to alive list via `atomicAdd(alive_count, 1)`

This rebuilds the alive list every frame (compact, no gaps).

### Pass 4: Render (Turitzin atomicAdd)
```
Shader:   particles_render.c_shd
Dispatch: ceil(alive_count / 256), 1, 1
Uniforms: camera_x, camera_y, canvas_w, canvas_h
RO:       buf_alive, buf_pos, buf_life, buf_color, buf_flags
RW:       buf_pixels
```
Each thread:
1. Read alive index → particle data
2. Transform world pos to screen pixel: `px = pos - camera + canvas/2`
3. Bounds check (skip if off-screen)
4. Compute final color (fade by normalized age: `life.x / life.y`)
5. Pack color as `uvec2(packHalf2x16(rg), packHalf2x16(ba))`
6. `atomicAdd(u_pixels.data[py * W + px].x, packed_rg)`
7. `atomicAdd(u_pixels.data[py * W + px].y, packed_ba)`

This is the core Turitzin insight: **additive blending via integer atomics** on
packed half-floats. Overlapping particles naturally accumulate — no sort needed,
no overdraw cost, no blend state changes.

**Note on atomicAdd with packed halfs**: True half-float atomicAdd requires
`NV_shader_atomic_int64` or similar. Fallback: use 4× uint buffer
(one uint per channel, fixed-point 8.8) and `atomicAdd` per channel.
This is portable and still fast at 480×270.

### Pass 5: Emit Light (Optional)
```
Shader:   particles_emit_light.c_shd
Dispatch: ceil(alive_count / 256), 1, 1
Uniforms: camera_x, camera_y, world_size
RO:       buf_alive, buf_pos, buf_life, buf_color, buf_flags
RW:       emissivity canvas (image2D, shared with HRC)
```
For particles with `emissive > 0`, write color × intensity into the HRC
emissivity canvas using `imageStore`. This happens **after**
`draw_emissivity()` but **before** `lighting_compute()`'s cascade passes,
so particle light participates in full GI (bounces off walls, casts shadows).

### Pass 6: Blit Pixel Buffer → Canvas
```
Shader:   particles_blit.c_shd
Dispatch: ceil(480*270 / 256), 1, 1
RO:       buf_pixels
RW:       output canvas (image2D)
```
Unpack each pixel's accumulated color and write to the output canvas via
`imageStore`. Alternatively, skip this and composite the pixel buffer
directly using CF's draw API with a custom fragment shader that reads the SSBO.

---

## Integration Points

### In World struct:
```c
typedef struct World {
    // ... existing fields ...
    ParticleState particles;
} World;
```

### In game lifecycle:
```c
// game_init → particles_init(&state->world.particles)
// game_shutdown → particles_shutdown(&state->world.particles)
// game_hot_reload → particles survive (GPU buffers in ParticleState)
```

### In frame loop (game.c):
```
update_world()
  └─ ... existing systems ...
  └─ particles_update(&world.particles, dt, emitters)  // kicks Pass 1-3

render_world()
  └─ PASS 1: render game scene to canvas
  └─ particles_render(&world.particles, camera)         // kicks Pass 4
  └─ particles_emit_light(&world.particles, &lighting)  // kicks Pass 5
  └─ PASS 2: HRC compute (now includes particle light)
  └─ PASS 3: composite fluence + particle output over canvas
```

### Particle output compositing:
The particle output canvas uses **additive blend** over the game canvas:
```c
CF_RenderState rs = cf_render_state_defaults();
rs.blend.rgb_src_blend_factor = CF_BLENDFACTOR_ONE;
rs.blend.rgb_dst_blend_factor = CF_BLENDFACTOR_ONE;
// Draw particle output canvas as fullscreen quad
```

This means particles glow naturally — bright particles bloom when composited
with the lit scene.

---

## Emitter Presets (Gameplay Use Cases)

```c
// Muzzle flash sparks — replaces/augments current MuzzleFlash
ParticleEmitter muzzle_sparks = {
    .spread = 0.5f, .speed_min = 80, .speed_max = 200,
    .life_min = 0.05f, .life_max = 0.15f,
    .r = 1.0f, .g = 0.8f, .b = 0.3f, .a = 1.0f,
    .emissive = 2.0f, .gravity = -200.0f, .drag = 3.0f,
    .spawn_count = 20,
};

// Shell casings
ParticleEmitter casings = {
    .spread = 0.3f, .speed_min = 40, .speed_max = 80,
    .life_min = 0.8f, .life_max = 1.5f,
    .r = 0.8f, .g = 0.7f, .b = 0.2f, .a = 1.0f,
    .emissive = 0.0f, .gravity = -400.0f, .drag = 1.0f,
    .spawn_count = 1,
};

// Bullet impact dust
ParticleEmitter impact_dust = {
    .spread = CF_PI, .speed_min = 10, .speed_max = 50,
    .life_min = 0.1f, .life_max = 0.4f,
    .r = 0.6f, .g = 0.5f, .b = 0.4f, .a = 0.5f,
    .emissive = 0.0f, .gravity = -50.0f, .drag = 5.0f,
    .spawn_count = 8,
};

// Blood spatter
ParticleEmitter blood = {
    .spread = 0.8f, .speed_min = 30, .speed_max = 120,
    .life_min = 0.3f, .life_max = 0.8f,
    .r = 0.6f, .g = 0.0f, .b = 0.0f, .a = 0.8f,
    .emissive = 0.0f, .gravity = -300.0f, .drag = 2.0f,
    .spawn_count = 12,
};

// Ambient firefly / ember
ParticleEmitter embers = {
    .spread = CF_PI, .speed_min = 5, .speed_max = 15,
    .life_min = 1.0f, .life_max = 3.0f,
    .r = 1.0f, .g = 0.5f, .b = 0.1f, .a = 0.6f,
    .emissive = 0.8f, .gravity = 20.0f, .drag = 1.0f,
    .spawn_count = 1,  // continuous trickle
};
```

---

## Portability: atomicAdd Strategy

The Turitzin article uses `atomicAdd` on packed half-floats (uvec2). Not all
GPUs support 64-bit atomics. Two portable approaches:

### Option A: 4× uint fixed-point (recommended for this game)
```glsl
// 4 uints per pixel: R, G, B, A as 16.16 fixed-point
layout(std430) buffer PixBuf { uint data[]; } u_pixels;  // W*H*4

// Write: convert float [0,1] → fixed 16.16, atomicAdd per channel
uint base = (py * W + px) * 4;
atomicAdd(u_pixels.data[base + 0], uint(r * 65536.0));
atomicAdd(u_pixels.data[base + 1], uint(g * 65536.0));
atomicAdd(u_pixels.data[base + 2], uint(b * 65536.0));
atomicAdd(u_pixels.data[base + 3], uint(a * 65536.0));
```
- Pros: works everywhere, 32-bit atomics are universally supported
- Cons: 4 atomic ops per particle instead of 2
- At 480×270 the buffer is 2 MB — still trivial
- 4 atomics is fine at this resolution; the Turitzin optimization matters
  more at high res with millions of particles

### Option B: uvec2 with 64-bit atomics (future optimization)
```glsl
// Requires GL_EXT_shader_atomic_int64 or equivalent
atomicAdd(u_pixels.data[py * W + px].x, packHalf2x16(vec2(r, g)));
atomicAdd(u_pixels.data[py * W + px].y, packHalf2x16(vec2(b, a)));
```
- Pros: 2 atomics, HDR-capable
- Cons: requires extension check; not portable to all mobile/integrated GPUs

**Recommendation**: Start with Option A. It's simple, portable, and the
performance difference is negligible at 480×270 with <16K particles.

---

## File Layout

```
src/game/
  particles.h          — ParticleState, ParticleEmitter, API
  particles.c          — init, shutdown, update, render, emit_light

assets/shaders/particles/
  particles_clear.c_shd    — zero pixel buffer
  particles_spawn.c_shd    — initialize new particles from emitter
  particles_update.c_shd   — physics integration + life management
  particles_render.c_shd   — atomicAdd into pixel buffer
  particles_emit_light.c_shd — scatter emissive particles into HRC
  particles_blit.c_shd     — unpack pixel buffer → output canvas
```

---

## Hot-Reload Safety

All GPU resources (storage buffers, canvases, compute shaders, materials) live
inside `ParticleState` which is a member of `World` inside `GameState`. The
`GameState*` pointer survives hot-reload. GPU buffer handles (which are just
integer IDs internally) remain valid across library reloads because they're
owned by the graphics backend, not the game library.

Compute shaders are loaded once in `particles_init()` and persist. If we want
shader hot-reload, we can add a `particles_reload_shaders()` call in
`game_hot_reload()` — same pattern as re-creating coroutines.

---

## Performance Budget

| Resource              | Size       | Notes                          |
|-----------------------|------------|--------------------------------|
| Particle SoA buffers  | 640 KB     | 16K × 40 bytes                 |
| Pool index buffers    | 192 KB     | 16K × 3 × 4 bytes             |
| Pixel buffer          | 2.0 MB     | 480×270 × 4 × uint            |
| Counters              | 8 bytes    | 2 × uint                      |
| **Total GPU memory**  | **~2.8 MB**|                                |

| Compute pass    | Threads  | Workgroups | Notes                    |
|-----------------|----------|------------|--------------------------|
| Clear pixels    | 129,600  | 507        | Trivial memset           |
| Spawn           | ~100     | 1-2        | Per emitter per frame    |
| Update          | 16,384   | 64         | All slots, skip dead     |
| Render          | ~alive   | ~variable  | Only living particles    |
| Emit light      | ~alive   | ~variable  | Only emissive particles  |
| Blit            | 129,600  | 507        | Unpack + imageStore      |

Total compute cost: ~6 dispatches, well under 0.5ms at 480×270.
