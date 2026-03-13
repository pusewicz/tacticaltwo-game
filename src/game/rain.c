// rain.c - Procedural rain storm effect
//
// Uses a 2D collision mask for rain occlusion (solid tiles block rain)
// and a 1D height map for splash positioning (topmost solid per column).

#include "rain.h"

#include <cute_alloc.h>
#include <cute_audio.h>
#include <cute_draw.h>
#include <cute_graphics.h>
#include <cute_math.h>
#include <stdint.h>
#include <string.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/log.h"
#include "ldtk.h"
#include "world.h"

// =============================================================================
// Texture Generation
// =============================================================================

static void build_textures(RainState* rain, LdtkLevel* level) {
  if (!level->int_grid || level->grid_width <= 0 || level->grid_height <= 0) {
    return;
  }

  int w = level->grid_width;
  int h = level->grid_height;

  // --- 2D collision mask (grid_width x grid_height) ---
  {
    int pixel_bytes = w * h * 4;
    uint8_t* buf =
        (uint8_t*)cf_arena_alloc(state->scratch_arena, pixel_bytes);

    for (int row = 0; row < h; row++) {
      for (int col = 0; col < w; col++) {
        int idx      = (row * w + col) * 4;
        bool solid   = level->int_grid[row * w + col] > 0;
        buf[idx + 0] = solid ? 255 : 0;
        buf[idx + 1] = 0;
        buf[idx + 2] = 0;
        buf[idx + 3] = 255;
      }
    }

    if (rain->mask_width != w || rain->mask_height != h) {
      if (rain->mask_width > 0) {
        cf_destroy_texture(rain->collision_mask);
      }
      CF_TextureParams params = cf_texture_defaults(w, h);
      params.filter           = CF_FILTER_NEAREST;
      params.wrap_u           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      params.wrap_v           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      rain->collision_mask    = cf_make_texture(params);
      rain->mask_width        = w;
      rain->mask_height       = h;
    }

    cf_texture_update(rain->collision_mask, buf, pixel_bytes);
  }

  // --- 1D height map (grid_width x 1) ---
  // R channel = grid row of topmost solid tile (255 = no ground)
  {
    int pixel_bytes = w * 4;
    uint8_t* buf =
        (uint8_t*)cf_arena_alloc(state->scratch_arena, pixel_bytes);

    for (int col = 0; col < w; col++) {
      uint8_t ground_row = 255;
      for (int row = 0; row < h; row++) {
        if (level->int_grid[row * w + col] > 0) {
          ground_row = (uint8_t)(row < 255 ? row : 254);
          break;
        }
      }
      buf[col * 4 + 0] = ground_row;
      buf[col * 4 + 1] = 0;
      buf[col * 4 + 2] = 0;
      buf[col * 4 + 3] = 255;
    }

    // Reuse mask_width to track — height map always matches collision mask width
    static bool height_map_created = false;
    if (!height_map_created) {
      CF_TextureParams params = cf_texture_defaults(w, 1);
      params.filter           = CF_FILTER_NEAREST;
      params.wrap_u           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      params.wrap_v           = CF_WRAP_MODE_CLAMP_TO_EDGE;
      rain->height_map        = cf_make_texture(params);
      height_map_created      = true;
    }

    cf_texture_update(rain->height_map, buf, pixel_bytes);
  }

  log_info("rain", "Built collision mask %dx%d + height map %d cols", w, h, w);
}

// =============================================================================
// Procedural Rain Audio
// =============================================================================

#define RAIN_SAMPLE_RATE  44100
#define RAIN_DURATION_SEC 3
#define RAIN_SAMPLE_COUNT (RAIN_SAMPLE_RATE * RAIN_DURATION_SEC) // 132300
#define RAIN_CROSSFADE    22050 // 500ms
#define RAIN_DEFAULT_MAX_VOLUME 0.6f
#define RAIN_DEFAULT_CUTOFF_HZ  4500.0f
#define RAIN_HIGHPASS_HZ        80.0f
#define RAIN_PI                 3.14159265f

static uint32_t xorshift32(uint32_t* s) {
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}

// Biquad filter state and coefficients
typedef struct {
  float b0, b1, b2, a1, a2;
  float x1, x2, y1, y2;
} Biquad;

static Biquad biquad_lowpass(float fc, float q, float sr) {
  float w0    = 2.0f * RAIN_PI * fc / sr;
  float sinw  = sinf(w0);
  float cosw  = cosf(w0);
  float alpha = sinw / (2.0f * q);
  float a0    = 1.0f + alpha;
  return (Biquad){
      .b0 = ((1.0f - cosw) / 2.0f) / a0,
      .b1 = (1.0f - cosw) / a0,
      .b2 = ((1.0f - cosw) / 2.0f) / a0,
      .a1 = (-2.0f * cosw) / a0,
      .a2 = (1.0f - alpha) / a0,
  };
}

static float biquad_process(Biquad* f, float x) {
  float y = f->b0 * x + f->b1 * f->x1 + f->b2 * f->x2 -
            f->a1 * f->y1 - f->a2 * f->y2;
  f->x2 = f->x1;
  f->x1 = x;
  f->y2 = f->y1;
  f->y1 = y;
  return y;
}

static void* generate_rain_wav(float cutoff_hz, int* out_size) {
  int data_bytes  = RAIN_SAMPLE_COUNT * 2; // 16-bit mono
  int total_bytes = 44 + data_bytes;
  uint8_t* buf    = (uint8_t*)cf_alloc((size_t)total_bytes);

  // RIFF WAV header
  memcpy(buf, "RIFF", 4);
  uint32_t chunk_size = (uint32_t)(total_bytes - 8);
  memcpy(buf + 4, &chunk_size, 4);
  memcpy(buf + 8, "WAVE", 4);
  memcpy(buf + 12, "fmt ", 4);
  uint32_t fmt_size    = 16;
  uint16_t audio_fmt   = 1; // PCM
  uint16_t channels    = 1;
  uint32_t sample_rate = RAIN_SAMPLE_RATE;
  uint32_t byte_rate   = RAIN_SAMPLE_RATE * 2;
  uint16_t block_align = 2;
  uint16_t bits        = 16;
  memcpy(buf + 16, &fmt_size, 4);
  memcpy(buf + 20, &audio_fmt, 2);
  memcpy(buf + 22, &channels, 2);
  memcpy(buf + 24, &sample_rate, 4);
  memcpy(buf + 28, &byte_rate, 4);
  memcpy(buf + 32, &block_align, 2);
  memcpy(buf + 34, &bits, 2);
  memcpy(buf + 36, "data", 4);
  uint32_t data_size = (uint32_t)data_bytes;
  memcpy(buf + 40, &data_size, 4);

  // Generate extra samples for overlap-add crossfade
  int gen_count = RAIN_SAMPLE_COUNT + RAIN_CROSSFADE;
  float* raw    = (float*)cf_alloc((size_t)gen_count * sizeof(float));

  // Pink noise state (Kellet economy filter)
  float pk0 = 0.0f, pk1 = 0.0f, pk2 = 0.0f;

  // Biquad lowpass (Butterworth, Q=0.7071)
  Biquad lp = biquad_lowpass(cutoff_hz, 0.7071f, (float)RAIN_SAMPLE_RATE);

  // One-pole highpass at 80 Hz to cut sub-bass rumble
  float hp_alpha  = 1.0f / (1.0f + (2.0f * RAIN_PI * RAIN_HIGHPASS_HZ) /
                                        (float)RAIN_SAMPLE_RATE);
  float hp_prev_x = 0.0f;
  float hp_prev_y = 0.0f;

  uint32_t rng_state = 0x12345678;

  for (int i = 0; i < gen_count; i++) {
    // White noise source
    float white =
        (float)xorshift32(&rng_state) / (float)UINT32_MAX * 2.0f - 1.0f;

    // Kellet economy pink noise filter (-3 dB/octave)
    pk0 = 0.99765f * pk0 + white * 0.0990460f;
    pk1 = 0.96300f * pk1 + white * 0.2965164f;
    pk2 = 0.57000f * pk2 + white * 1.0526913f;
    float pink = pk0 + pk1 + pk2 + white * 0.1848f;
    pink *= 0.11f; // normalize to roughly [-1, 1]

    // Biquad lowpass
    float filtered = biquad_process(&lp, pink);

    // One-pole highpass
    float hp_out = hp_alpha * (hp_prev_y + filtered - hp_prev_x);
    hp_prev_x    = filtered;
    hp_prev_y    = hp_out;

    // Slow amplitude modulation (organic breathing)
    float t  = (float)i / (float)RAIN_SAMPLE_RATE;
    float am = 1.0f -
               0.12f * (0.5f - 0.5f * sinf(2.0f * RAIN_PI * 0.31f * t)) -
               0.12f * (0.5f - 0.5f * sinf(2.0f * RAIN_PI * 0.47f * t)) -
               0.12f * (0.5f - 0.5f * sinf(2.0f * RAIN_PI * 0.73f * t));

    raw[i] = hp_out * am;
  }

  // Overlap-add crossfade: blend the tail extension into the head
  // so sample[N-1] flows seamlessly into sample[0] on loop
  int16_t* samples = (int16_t*)(buf + 44);
  for (int i = 0; i < RAIN_CROSSFADE; i++) {
    float t = (float)i / (float)RAIN_CROSSFADE;
    raw[i]  = raw[i] * t + raw[RAIN_SAMPLE_COUNT + i] * (1.0f - t);
  }

  // Convert to 16-bit PCM
  for (int i = 0; i < RAIN_SAMPLE_COUNT; i++) {
    int32_t s = (int32_t)(raw[i] * 20000.0f);
    if (s > 32767) s = 32767;
    if (s < -32767) s = -32767;
    samples[i] = (int16_t)s;
  }

  cf_free(raw);

  *out_size = total_bytes;
  return buf;
}

// =============================================================================
// Public API
// =============================================================================

void rain_init(void) {
  RainState* rain = &state->world.rain;
  memset(rain, 0, sizeof(RainState));

  rain->shader    = cf_make_draw_shader("rain.shd");
  rain->intensity = 1.0f;
  rain->density   = 1.0f;
  rain->alpha     = 1.0f;
  rain->speed     = 5.0f;
  rain->wind      = 0.05f;

  LdtkMap* map = &state->world.map;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    build_textures(rain, &map->levels[map->active_level]);
  }

  rain->cutoff_hz        = RAIN_DEFAULT_CUTOFF_HZ;
  rain->max_volume       = RAIN_DEFAULT_MAX_VOLUME;
  rain->splash_cell_size = 24.0f;
  rain->splash_rate      = 4.0f;
  rain->splash_life      = 1.0f;
  rain->splash_speed     = 20.0f;
  rain->splash_gravity   = 20.0f;
  rain->initialized = true;

  // Generate procedural rain audio and start looped playback
  int wav_size   = 0;
  void* wav_data = generate_rain_wav(rain->cutoff_hz, &wav_size);
  rain->audio    = cf_audio_load_wav_from_memory(wav_data, wav_size);
  cf_free(wav_data);

  CF_SoundParams params = cf_sound_params_defaults();
  params.looped         = true;
  params.volume         = rain->intensity * rain->max_volume;
  rain->sound           = cf_play_sound(rain->audio, params);
  rain->audio_playing   = true;

  log_info("rain", "Rain system initialized (intensity=%.1f)",
           (double)rain->intensity);
}

void rain_rebuild_height_map(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized) {
    return;
  }

  LdtkMap* map = &state->world.map;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    build_textures(rain, &map->levels[map->active_level]);
  }
}

void rain_rebuild_audio(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized) {
    return;
  }

  if (rain->audio_playing) {
    cf_sound_stop(rain->sound);
    rain->audio_playing = false;
  }
  cf_audio_destroy(rain->audio);

  int wav_size   = 0;
  void* wav_data = generate_rain_wav(rain->cutoff_hz, &wav_size);
  rain->audio    = cf_audio_load_wav_from_memory(wav_data, wav_size);
  cf_free(wav_data);

  CF_SoundParams params = cf_sound_params_defaults();
  params.looped         = true;
  params.volume         = rain->intensity * rain->max_volume;
  rain->sound           = cf_play_sound(rain->audio, params);
  rain->audio_playing   = true;
}

void rain_update_audio(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized || !rain->audio_playing) {
    return;
  }

  float volume = rain->intensity * rain->max_volume;
  cf_sound_set_volume(rain->sound, volume);

  if (rain->intensity <= 0.001f) {
    cf_sound_set_is_paused(rain->sound, true);
  } else {
    cf_sound_set_is_paused(rain->sound, false);
  }
}

void rain_render(float dt) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized || rain->intensity <= 0.001f) {
    return;
  }

  rain->time += dt;

  LdtkMap* map     = &state->world.map;
  LdtkLevel* level = nullptr;
  if (map->loaded && map->active_level >= 0 &&
      map->active_level < map->level_count) {
    level = &map->levels[map->active_level];
  }

  float hw = (float)CANVAS_WIDTH / 2.0f;
  float hh = (float)CANVAS_HEIGHT / 2.0f;
  CF_Aabb fullscreen = {.min = cf_v2(-hw, -hh), .max = cf_v2(hw, hh)};

  // Atmospheric tint
  {
    float tint_alpha = 0.35f * rain->intensity;
    cf_draw_push_color(
        cf_make_color_rgba_f(0.08f, 0.06f, 0.18f, tint_alpha));
    cf_draw_box_fill(fullscreen, 0);
    cf_draw_pop_color();
  }

  // Rain shader overlay
  if (rain->mask_width > 0 && level != nullptr) {
    cf_draw_push_shader(rain->shader);
    cf_draw_set_uniform_float("u_time", rain->time);
    cf_draw_set_uniform_float("u_intensity", rain->intensity);
    cf_draw_set_uniform_float("u_density", rain->density);
    cf_draw_set_uniform_float("u_alpha", rain->alpha);
    cf_draw_set_uniform_float("u_speed", rain->speed);
    cf_draw_set_uniform_float("u_wind", rain->wind);
    cf_draw_set_uniform_v2("u_camera", state->world.camera);
    cf_draw_set_uniform_v2("u_canvas_size",
                           cf_v2((float)CANVAS_WIDTH, (float)CANVAS_HEIGHT));
    cf_draw_set_uniform_v2(
        "u_level_size",
        cf_v2((float)level->width, (float)level->height));
    cf_draw_set_uniform_float("u_grid_size", (float)LDTK_GRID_SIZE);
    cf_draw_set_uniform_float("u_grid_width", (float)rain->mask_width);
    cf_draw_set_uniform_float("u_grid_height", (float)rain->mask_height);
    cf_draw_set_uniform_float("u_splash_cell_size", rain->splash_cell_size);
    cf_draw_set_uniform_float("u_splash_rate", rain->splash_rate);
    cf_draw_set_uniform_float("u_splash_life", rain->splash_life);
    cf_draw_set_uniform_float("u_splash_speed", rain->splash_speed);
    cf_draw_set_uniform_float("u_splash_gravity", rain->splash_gravity);
    cf_draw_set_texture("collision_tex", rain->collision_mask);
    cf_draw_set_texture("height_tex", rain->height_map);

    cf_draw_push_color(cf_color_white());
    cf_draw_box_fill(fullscreen, 0);
    cf_draw_pop_color();

    cf_draw_pop_shader();
  }
}

void rain_shutdown(void) {
  RainState* rain = &state->world.rain;
  if (!rain->initialized) {
    return;
  }

  if (rain->audio_playing) {
    cf_sound_stop(rain->sound);
    rain->audio_playing = false;
  }
  cf_audio_destroy(rain->audio);

  if (rain->mask_width > 0) {
    cf_destroy_texture(rain->collision_mask);
    cf_destroy_texture(rain->height_map);
  }
  cf_destroy_shader(rain->shader);
  rain->initialized = false;

  log_info("rain", "Rain system shut down");
}
