// rain_system.c - Procedural rain ambient audio
//
// Generates white noise, applies a single-pole low-pass filter to produce
// a soft rain sound, packs it into an in-memory WAV, and loops it.

#include <cute_audio.h>
#include <stdlib.h>
#include <string.h>

#include "../../engine/game_state.h"
#include "../../engine/log.h"
#include "../world.h"
#include "systems.h"

// Rain noise parameters
#define RAIN_SAMPLE_RATE 44100
#define RAIN_DURATION_SEC 4
#define RAIN_SAMPLE_COUNT (RAIN_SAMPLE_RATE * RAIN_DURATION_SEC)
#define RAIN_CUTOFF_HZ 900.0f
#define RAIN_VOLUME 0.3f

// Simple xorshift32 PRNG for deterministic white noise
static uint32_t xorshift32(uint32_t* s) {
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *s = x;
  return x;
}

// Write a little-endian uint16 into a byte buffer
static void write_u16(uint8_t* buf, uint16_t val) {
  buf[0] = (uint8_t)(val & 0xFF);
  buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

// Write a little-endian uint32 into a byte buffer
static void write_u32(uint8_t* buf, uint32_t val) {
  buf[0] = (uint8_t)(val & 0xFF);
  buf[1] = (uint8_t)((val >> 8) & 0xFF);
  buf[2] = (uint8_t)((val >> 16) & 0xFF);
  buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

// Build WAV file bytes from 16-bit mono PCM samples.
// Caller must free the returned buffer.
static uint8_t* build_wav(const int16_t* samples, int sample_count,
                          int* out_size) {
  uint32_t data_size = (uint32_t)sample_count * 2; // 16-bit = 2 bytes/sample
  uint32_t file_size = 44 + data_size;             // 44-byte header + data
  uint8_t* buf       = malloc(file_size);
  if (!buf) {
    *out_size = 0;
    return nullptr;
  }

  // RIFF header
  memcpy(buf + 0, "RIFF", 4);
  write_u32(buf + 4, file_size - 8);
  memcpy(buf + 8, "WAVE", 4);

  // fmt chunk
  memcpy(buf + 12, "fmt ", 4);
  write_u32(buf + 16, 16);          // chunk size
  write_u16(buf + 20, 1);           // PCM format
  write_u16(buf + 22, 1);           // mono
  write_u32(buf + 24, RAIN_SAMPLE_RATE);
  write_u32(buf + 28, RAIN_SAMPLE_RATE * 2); // byte rate
  write_u16(buf + 32, 2);           // block align
  write_u16(buf + 34, 16);          // bits per sample

  // data chunk
  memcpy(buf + 36, "data", 4);
  write_u32(buf + 40, data_size);
  memcpy(buf + 44, samples, data_size);

  *out_size = (int)file_size;
  return buf;
}

void sys_rain_init(void) {
  Rain* rain = &state->world.rain;

  // Generate white noise samples
  int16_t* samples = malloc(RAIN_SAMPLE_COUNT * sizeof(int16_t));
  if (!samples) {
    log_warn("rain", "Failed to allocate noise buffer");
    return;
  }

  uint32_t rng = 0xDEADBEEF;
  for (int i = 0; i < RAIN_SAMPLE_COUNT; i++) {
    // Random float in [-1, 1]
    float noise = ((float)(xorshift32(&rng) & 0xFFFF) / 32768.0f) - 1.0f;
    samples[i]  = (int16_t)(noise * 32767.0f);
  }

  // Apply single-pole low-pass filter (IIR):
  //   y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
  // where alpha = dt / (RC + dt), RC = 1 / (2*pi*cutoff)
  {
    float dt    = 1.0f / (float)RAIN_SAMPLE_RATE;
    float rc    = 1.0f / (2.0f * 3.14159265f * RAIN_CUTOFF_HZ);
    float alpha = dt / (rc + dt);
    float prev  = 0.0f;

    for (int i = 0; i < RAIN_SAMPLE_COUNT; i++) {
      float x = (float)samples[i] / 32767.0f;
      prev    = alpha * x + (1.0f - alpha) * prev;
      samples[i] = (int16_t)(prev * 32767.0f);
    }
  }

  // Cross-fade the last 2048 samples with the first 2048 for seamless looping
  {
    int fade_len = 2048;
    for (int i = 0; i < fade_len; i++) {
      float t     = (float)i / (float)fade_len;
      int tail    = RAIN_SAMPLE_COUNT - fade_len + i;
      float a     = (float)samples[tail] / 32767.0f;
      float b     = (float)samples[i] / 32767.0f;
      float mixed = a * (1.0f - t) + b * t;
      samples[tail] = (int16_t)(mixed * 32767.0f);
    }
  }

  // Pack into WAV and load
  int wav_size    = 0;
  uint8_t* wav    = build_wav(samples, RAIN_SAMPLE_COUNT, &wav_size);
  free(samples);

  if (!wav) {
    log_warn("rain", "Failed to build WAV buffer");
    return;
  }

  rain->audio = cf_audio_load_wav_from_memory(wav, wav_size);
  free(wav);

  // Play looped at low volume
  CF_SoundParams params = cf_sound_params_defaults();
  params.looped         = true;
  params.volume         = RAIN_VOLUME;
  rain->sound           = cf_play_sound(rain->audio, params);
  rain->playing         = true;

  log_info("rain", "Rain ambient started (%d samples, cutoff %dHz)",
           RAIN_SAMPLE_COUNT, (int)RAIN_CUTOFF_HZ);
}

void sys_rain_shutdown(void) {
  Rain* rain = &state->world.rain;
  if (rain->playing) {
    cf_sound_stop(rain->sound);
    rain->playing = false;
  }
  if (rain->audio.id != 0) {
    cf_audio_destroy(rain->audio);
    rain->audio.id = 0;
  }
}
