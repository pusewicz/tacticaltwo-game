# Rain Noise Filter - Implementation Plan

Enhance the existing procedural rain audio system with better noise shaping,
multi-band filtering, and runtime controls.

## Current State

The rain system (`src/game/systems/rain_system.c`) generates white noise,
applies a single-pole IIR low-pass filter at 900 Hz, cross-fades for seamless
looping, packs into an in-memory WAV, and plays it looped at 0.3 volume.

**What's lacking:**
- Single filter stage sounds thin/harsh - real rain has complex spectral shape
- No runtime volume or intensity control
- No fade in/out transitions
- Fixed filter cutoff - can't vary rain character (drizzle vs downpour)

---

## Step 1: Add a Second Filter Stage (Biquad Low-Pass)

Replace the single-pole IIR with a biquad (2nd-order) filter for a steeper
rolloff and warmer tone.

**File:** `src/game/systems/rain_system.c`

### 1.1 Add biquad coefficients struct

After the existing `#define` constants (~line 20), add:

```c
// Biquad filter coefficients (2nd-order IIR)
typedef struct {
  float b0, b1, b2; // feedforward
  float a1, a2;     // feedback (a0 normalized to 1)
  float x1, x2;     // input history
  float y1, y2;     // output history
} Biquad;
```

### 1.2 Add biquad initialization function

```c
// Initialize a low-pass biquad filter
// Reference: Audio EQ Cookbook (Robert Bristow-Johnson)
static Biquad biquad_lowpass(float cutoff_hz, float q, float sample_rate) {
  float w0    = 2.0f * 3.14159265f * cutoff_hz / sample_rate;
  float alpha = sinf(w0) / (2.0f * q);
  float cos_w0 = cosf(w0);

  float a0 = 1.0f + alpha;
  Biquad bq = {
    .b0 = ((1.0f - cos_w0) / 2.0f) / a0,
    .b1 = (1.0f - cos_w0) / a0,
    .b2 = ((1.0f - cos_w0) / 2.0f) / a0,
    .a1 = (-2.0f * cos_w0) / a0,
    .a2 = (1.0f - alpha) / a0,
    .x1 = 0, .x2 = 0, .y1 = 0, .y2 = 0,
  };
  return bq;
}
```

### 1.3 Add biquad process function

```c
static float biquad_process(Biquad* bq, float x) {
  float y = bq->b0 * x + bq->b1 * bq->x1 + bq->b2 * bq->x2
          - bq->a1 * bq->y1 - bq->a2 * bq->y2;
  bq->x2 = bq->x1; bq->x1 = x;
  bq->y2 = bq->y1; bq->y1 = y;
  return y;
}
```

### 1.4 Replace the existing filter block in `sys_rain_init()`

Replace lines 99-113 (the single-pole IIR block) with:

```c
// Apply cascaded biquad low-pass filters for smooth rain spectrum
{
  Biquad lp1 = biquad_lowpass(RAIN_CUTOFF_HZ, 0.707f, (float)RAIN_SAMPLE_RATE);
  Biquad lp2 = biquad_lowpass(RAIN_CUTOFF_HZ * 0.6f, 0.707f, (float)RAIN_SAMPLE_RATE);

  for (int i = 0; i < RAIN_SAMPLE_COUNT; i++) {
    float x = (float)samples[i] / 32767.0f;
    x = biquad_process(&lp1, x);
    x = biquad_process(&lp2, x);
    // Soft-clip to prevent overflow
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    samples[i] = (int16_t)(x * 32767.0f);
  }
}
```

**Why two stages:** The first filter at 900 Hz removes harsh highs. The second
at 540 Hz (0.6x) further shapes the spectrum toward the low rumble of rain
on a surface. Q=0.707 (Butterworth) gives flat passband, no resonance.

### 1.5 Add `<math.h>` include

Add at the top of the file (for `sinf`, `cosf`):

```c
#include <math.h>
```

- [ ] Implement biquad struct and functions
- [ ] Replace single-pole filter with cascaded biquad
- [ ] Build and listen - verify warmer, deeper tone

---

## Step 2: Add Pink Noise Shaping (Optional Enhancement)

Real rain is closer to pink noise (1/f spectrum) than filtered white noise.
Add a Voss-McCartney pink noise approximation.

**File:** `src/game/systems/rain_system.c`

### 2.1 Add pink noise generator

Replace the white noise generation (lines 92-97) with a pink noise algorithm:

```c
#define PINK_OCTAVES 5

// Generate pink noise using Voss-McCartney algorithm
static void generate_pink_noise(int16_t* samples, int count, uint32_t* rng) {
  float octave_values[PINK_OCTAVES] = {0};
  float running_sum = 0.0f;

  for (int i = 0; i < count; i++) {
    // Update one octave per sample based on bit pattern
    for (int j = 0; j < PINK_OCTAVES; j++) {
      // Update octave j every 2^j samples
      if ((i & ((1 << j) - 1)) == 0) {
        running_sum -= octave_values[j];
        float r = ((float)(xorshift32(rng) & 0xFFFF) / 32768.0f) - 1.0f;
        octave_values[j] = r;
        running_sum += r;
      }
    }
    // Add white noise for highest frequencies
    float white = ((float)(xorshift32(rng) & 0xFFFF) / 32768.0f) - 1.0f;
    float pink  = (running_sum + white) / (float)(PINK_OCTAVES + 1);
    samples[i]  = (int16_t)(pink * 32767.0f);
  }
}
```

### 2.2 Call it from `sys_rain_init()`

Replace the white noise loop (lines 92-97) with:

```c
uint32_t rng = 0xDEADBEEF;
generate_pink_noise(samples, RAIN_SAMPLE_COUNT, &rng);
```

- [ ] Implement pink noise generator
- [ ] Update `sys_rain_init()` to use it
- [ ] Build and compare: pink noise should sound more natural before filtering

---

## Step 3: Rain Intensity Parameter

Add a rain intensity value to the `Rain` struct so the sound character can
vary at runtime (e.g., heavier rain = louder + more high-frequency content).

### 3.1 Extend the `Rain` struct

**File:** `src/game/world.h` (line 120-124)

```c
typedef struct Rain {
  CF_Audio audio;
  CF_Sound sound;
  bool playing;
  float intensity; // 0.0 = silence, 1.0 = full downpour
} Rain;
```

### 3.2 Add rain intensity constants

**File:** `src/game/systems/rain_system.c`

```c
#define RAIN_INTENSITY_DEFAULT 0.7f
#define RAIN_VOLUME_MIN 0.05f
#define RAIN_VOLUME_MAX 0.5f
```

### 3.3 Set initial intensity in `sys_rain_init()`

After `rain->playing = true;` (line 146):

```c
rain->intensity = RAIN_INTENSITY_DEFAULT;
```

### 3.4 Add a runtime update function

**File:** `src/game/systems/rain_system.c`

```c
void sys_rain_update(void) {
  Rain* rain = &state->world.rain;
  if (!rain->playing) return;

  // Map intensity to volume: lerp between min and max
  float vol = RAIN_VOLUME_MIN + rain->intensity * (RAIN_VOLUME_MAX - RAIN_VOLUME_MIN);
  cf_sound_set_volume(rain->sound, vol);
}
```

### 3.5 Declare and wire up

**File:** `src/game/systems/systems.h` - add declaration:
```c
void sys_rain_update(void);
```

**File:** `src/game/world.c` - add to `update_world()`:
```c
sys_rain_update();
```

- [ ] Extend `Rain` struct with `intensity`
- [ ] Add volume mapping constants
- [ ] Implement `sys_rain_update()`
- [ ] Wire into update loop
- [ ] Test: modify `intensity` value and verify volume changes

---

## Step 4: Fade In/Out Transitions

Smooth audio transitions when starting/stopping rain.

### 4.1 Add fade state to `Rain` struct

**File:** `src/game/world.h`

```c
typedef struct Rain {
  CF_Audio audio;
  CF_Sound sound;
  bool playing;
  float intensity;
  float fade_target;   // target volume multiplier (0=silent, 1=full)
  float fade_current;  // current volume multiplier
  float fade_speed;    // units per second
} Rain;
```

### 4.2 Initialize fade state in `sys_rain_init()`

```c
rain->fade_target  = 1.0f;
rain->fade_current = 0.0f; // start silent, fade in
rain->fade_speed   = 0.5f; // 2 seconds to full volume
```

### 4.3 Update `sys_rain_update()` to handle fading

```c
void sys_rain_update(void) {
  Rain* rain = &state->world.rain;
  if (!rain->playing) return;

  // Lerp fade_current toward fade_target
  float dt = state->world.dt;
  if (rain->fade_current < rain->fade_target) {
    rain->fade_current += rain->fade_speed * dt;
    if (rain->fade_current > rain->fade_target)
      rain->fade_current = rain->fade_target;
  } else if (rain->fade_current > rain->fade_target) {
    rain->fade_current -= rain->fade_speed * dt;
    if (rain->fade_current < rain->fade_target)
      rain->fade_current = rain->fade_target;
  }

  float vol = RAIN_VOLUME_MIN
            + rain->intensity * (RAIN_VOLUME_MAX - RAIN_VOLUME_MIN);
  vol *= rain->fade_current;
  cf_sound_set_volume(rain->sound, vol);
}
```

### 4.4 Add helper to trigger fade out

```c
void sys_rain_fade_out(float duration) {
  Rain* rain = &state->world.rain;
  rain->fade_target = 0.0f;
  rain->fade_speed  = 1.0f / duration;
}
```

- [ ] Add fade fields to `Rain` struct
- [ ] Initialize fade state (start faded out, fade in)
- [ ] Implement fade logic in `sys_rain_update()`
- [ ] Add `sys_rain_fade_out()` helper
- [ ] Test: verify smooth fade-in on game start

---

## Step 5: Build and Verify

### 5.1 Build

```bash
rake
```

Remember: `ninja: no work to do.` means success. Don't retry.

### 5.2 Run and listen

```bash
rake run
```

### 5.3 Verify checklist

- [ ] No compiler warnings (strict `-Wall -Wextra -Wpedantic`)
- [ ] Rain fades in smoothly on startup (no pop/click)
- [ ] Sound is warmer/deeper than original single-pole filter
- [ ] No audio glitches or clicks at loop point
- [ ] Hot reload still works (rain survives library reload)

---

## Summary of Files to Modify

| File | Changes |
|------|---------|
| `src/game/systems/rain_system.c` | Biquad filter, pink noise, intensity, fade logic |
| `src/game/world.h` | Extend `Rain` struct with `intensity`, fade fields |
| `src/game/systems/systems.h` | Declare `sys_rain_update()`, `sys_rain_fade_out()` |
| `src/game/world.c` | Call `sys_rain_update()` in `update_world()` |

## Key References

- **Audio EQ Cookbook**: Robert Bristow-Johnson's biquad formulas
- **Cute Framework audio API**: `cf_audio_load_wav_from_memory()`, `cf_play_sound()`, `cf_sound_set_volume()`
- **Existing implementation**: `src/game/systems/rain_system.c` lines 82-162
