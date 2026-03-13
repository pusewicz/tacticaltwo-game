// lighting.h - HRC 2D global illumination
//
// Implements Holographic Radiance Cascades (HRC) for realistic 2D GI.
// Light sources are collected each frame (static from LDtk, dynamic from C)
// and composited over the game canvas using multiply blend.
//
// Reference: Freeman, Sannikov, Margel (2025) "Holographic Radiance Cascades"
// https://arxiv.org/pdf/2505.02041

#pragma once

#include <cute_graphics.h>
#include <stdbool.h>

#include "ldtk.h"

// =============================================================================
// Configuration
// =============================================================================

#define LIGHTING_WORLD_SIZE 512
#define LIGHTING_GRID_SIZE  512
#define LIGHTING_MAX_LIGHTS 64

// log2(LIGHTING_WORLD_SIZE) = 9
#define LIGHTING_MAX_N 9

// =============================================================================
// Light source
// =============================================================================

typedef struct Light {
  float x, y;       // world position
  float r, g, b;    // emissive color
  float intensity;  // brightness multiplier
  float radius;     // falloff radius in game pixels
  float direction;  // cone aim direction in degrees (0=right, 90=up, 270=down)
  float cone;       // full cone spread in degrees (360 = omnidirectional)
} Light;

// =============================================================================
// LightingState
// =============================================================================

typedef struct LightingState {
  // Scene input canvases (LIGHTING_WORLD_SIZE x LIGHTING_WORLD_SIZE, f16)
  CF_Canvas emissivity;
  CF_Canvas absorption;

  // Final GI output canvas (LIGHTING_WORLD_SIZE x LIGHTING_WORLD_SIZE, RGBA8)
  CF_Canvas fluence;

  // Per-cascade virtual ray (T) SSBOs — f16-packed uvec2, 8 bytes/element
  CF_StorageBuffer vrays_rad[LIGHTING_MAX_N + 1];
  CF_StorageBuffer vrays_trn[LIGHTING_MAX_N + 1];

  // Radiance accumulation (R) ping-pong buffers + zero buffer (R_N = 0)
  CF_StorageBuffer r_rad[2];
  CF_StorageBuffer r_zero;

  // Per-rotation (4) frustum output buffers
  CF_StorageBuffer frustum[4];

  // Min/max absorption and prefiltered inputs
  CF_Canvas minmax;
  CF_Canvas emissivity_filtered;
  CF_Canvas absorption_filtered;

  // CF_Material per shader pass
  CF_Material mat_trace;
  CF_Material mat_extend;
  CF_Material mat_merge;
  CF_Material mat_composite;
  CF_Material mat_copy;
  CF_Material mat_minmax;
  CF_Material mat_prefilter;

  // Compute shaders
  CF_ComputeShader cs_seed;
  CF_ComputeShader cs_trace;
  CF_ComputeShader cs_extend;
  CF_ComputeShader cs_merge;
  CF_ComputeShader cs_copy;
  CF_ComputeShader cs_composite;
  CF_ComputeShader cs_minmax;
  CF_ComputeShader cs_prefilter;
  CF_ComputeShader cs_prefilter_gauss;

  // Precomputed cascade buffer widths (at current grid resolution)
  int vrays_w[LIGHTING_MAX_N + 1];

  // Per-frame light list (reset each frame)
  Light lights[LIGHTING_MAX_LIGHTS];
  int light_count;

  // Config
  float ambient;      // 0.0 (night) → 1.0 (full daylight)
  int grid;           // cascade resolution (64 or 128)
  int n;              // log2(grid)
  int trace_levels;   // cascade levels using direct trace (0..n+1)
  int cminus1;        // c-1 gathering enabled
  int upscale_mode;   // 0=nearest, 1=minmax bilinear, 2=joint bilateral
  int blend_boundary; // angle-based weight blending near ±45° edges
  float blend_width;  // blend falloff width in direction slots
  int prefilter_mode; // 0=off, 1=box, 2=mipmap, 3=gauss, 4=mip+max
  int debug_mode;     // 0=normal, 1-4=individual frustums, 5=all, 6=emissivity,
                      // 7=absorption

  bool initialized;
} LightingState;

// =============================================================================
// API
// =============================================================================

// Initialize GPU resources and load compute shaders.
void lighting_init(LightingState* state);

// Free GPU resources.
void lighting_shutdown(LightingState* state);

// Reset light list. Call at the start of each frame before adding lights.
void lighting_begin_frame(LightingState* state);

// Add a light source for this frame (world coordinates).
void lighting_add_light(LightingState* state, Light light);

// Render emissivity pass + run HRC compute pipeline → updates fluence.
// camera: top-left world position of current viewport.
void lighting_compute(LightingState* state, float camera_x, float camera_y);

// Build absorption texture from level int_grid.
// Must be called with the current camera position each frame (before
// lighting_compute) so that both absorption and emissivity use the same
// camera-relative coordinate space.
void lighting_build_absorption(LightingState* state, LdtkMap* map,
                               float camera_x, float camera_y);

// Return the fluence canvas for compositing.
CF_Canvas lighting_fluence_canvas(LightingState* state);

// Change cascade grid resolution (64 or 128). Clamps trace_levels.
void lighting_set_grid(LightingState* state, int grid);
