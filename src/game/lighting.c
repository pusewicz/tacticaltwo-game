// lighting.c - HRC 2D global illumination
//
// Ported from vendor/cute_framework/samples/hrc.c with:
//   - World size reduced from 1024 to LIGHTING_WORLD_SIZE (256)
//   - Emissivity drawn from per-frame light list (not demo orbs)
//   - Absorption built from LDtk int_grid on level load
//   - State passed as pointer (not global)

#include "lighting.h"

#include <cute_alloc.h>
#include <cute_draw.h>
#include <cute_file_system.h>
#include <cute_graphics.h>
#include <math.h>

#include "../config/config.h"
#include "../engine/log.h"

// =============================================================================
// Helpers (mirrored from hrc.c)
// =============================================================================

static int lighting_div_ceil(int a, int b) { return (a + b - 1) / b; }

#define HRC_WG 16

void lighting_set_grid(LightingState* s, int grid)
{
  s->grid = grid;
  s->n    = (int)log2f((float)grid);
  if (s->trace_levels > s->n + 1) {
    s->trace_levels = s->n + 1;
  }
  for (int i = 0; i <= s->n; i++) {
    int rays   = (1 << i) + 1;
    int probes = grid >> i;
    s->vrays_w[i] = probes * rays;
  }
}

static CF_StorageBuffer lighting_make_buf(int w, int h)
{
  CF_StorageBufferParams p = cf_storage_buffer_defaults(w * h * 8);
  p.compute_readable = true;
  p.compute_writable = true;
  return cf_make_storage_buffer(p);
}

static CF_Canvas lighting_make_canvas(int w, int h, CF_PixelFormat fmt,
                                       CF_Filter filter, bool mipmaps)
{
  CF_CanvasParams p        = cf_canvas_defaults(w, h);
  p.target.pixel_format    = fmt;
  p.target.filter          = filter;
  p.target.usage           = CF_TEXTURE_USAGE_SAMPLER_BIT
                           | CF_TEXTURE_USAGE_COLOR_TARGET_BIT
                           | CF_TEXTURE_USAGE_COMPUTE_STORAGE_READ_BIT
                           | CF_TEXTURE_USAGE_COMPUTE_STORAGE_WRITE_BIT;
  p.target.wrap_u          = CF_WRAP_MODE_CLAMP_TO_EDGE;
  p.target.wrap_v          = CF_WRAP_MODE_CLAMP_TO_EDGE;
  if (mipmaps) {
    p.target.allocate_mipmaps = true;
    p.target.mip_count        = 0;
  }
  return cf_make_canvas(p);
}

static CF_ComputeShader lighting_load_shader(const char* path)
{
  char* src = cf_fs_read_entire_file_to_memory_and_nul_terminate(path, nullptr);
  CF_ComputeShader cs = cf_make_compute_shader_from_source(src);
  cf_free(src);
  return cs;
}

// =============================================================================
// Init / Shutdown
// =============================================================================

void lighting_init(LightingState* s)
{
  CF_MEMSET(s, 0, sizeof(*s));

  lighting_set_grid(s, LIGHTING_GRID_SIZE);
  s->trace_levels   = 3;
  s->cminus1        = 1;
  s->upscale_mode   = 1;
  s->blend_boundary = 1;
  s->blend_width    = 4.0f;
  s->prefilter_mode = 2;
  s->ambient        = 0.05f; // dim moonlit night — just enough to see silhouettes

  // Precompute max T cascade buffer widths for allocation.
  int max_vrays_w[LIGHTING_MAX_N + 1];
  for (int i = 0; i <= LIGHTING_MAX_N; i++) {
    int rays           = (1 << i) + 1;
    int probes         = LIGHTING_WORLD_SIZE >> i;
    max_vrays_w[i]     = probes * rays;
  }

  // Scene input canvases: f16 RGBA, nearest filter, mipmaps for prefilter mode.
  s->emissivity = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R16G16B16A16_FLOAT, CF_FILTER_NEAREST, true);
  s->absorption = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R16G16B16A16_FLOAT, CF_FILTER_NEAREST, true);

  // Per-cascade T SSBOs allocated at maximum size.
  for (int i = 0; i <= LIGHTING_MAX_N; i++) {
    s->vrays_rad[i] = lighting_make_buf(max_vrays_w[i], LIGHTING_WORLD_SIZE);
    s->vrays_trn[i] = lighting_make_buf(max_vrays_w[i], LIGHTING_WORLD_SIZE);
  }

  // R ping-pong + zero buffer.
  for (int i = 0; i < 2; i++)
    s->r_rad[i] = lighting_make_buf(LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE);
  s->r_zero = lighting_make_buf(LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE);
  {
    int   sz    = LIGHTING_WORLD_SIZE * LIGHTING_WORLD_SIZE * 8;
    void* zeros = cf_calloc((size_t)sz, 1);
    cf_update_storage_buffer(s->r_zero, zeros, sz);
    cf_free(zeros);
  }

  // Per-rotation frustum output SSBOs.
  for (int i = 0; i < 4; i++)
    s->frustum[i] = lighting_make_buf(LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE);

  // Fluence output (RGBA8, linear filter for smooth display).
  s->fluence = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R8G8B8A8_UNORM, CF_FILTER_LINEAR, false);

  // Min/max and prefiltered canvases.
  s->minmax = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R16G16_FLOAT, CF_FILTER_NEAREST, false);
  s->emissivity_filtered = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R16G16B16A16_FLOAT, CF_FILTER_NEAREST, false);
  s->absorption_filtered = lighting_make_canvas(
      LIGHTING_WORLD_SIZE, LIGHTING_WORLD_SIZE,
      CF_PIXEL_FORMAT_R16G16B16A16_FLOAT, CF_FILTER_NEAREST, false);

  // Materials.
  s->mat_trace     = cf_make_material();
  s->mat_extend    = cf_make_material();
  s->mat_merge     = cf_make_material();
  s->mat_composite = cf_make_material();
  s->mat_copy      = cf_make_material();
  s->mat_minmax    = cf_make_material();
  s->mat_prefilter = cf_make_material();

  // Compute shaders from assets/shaders/hrc/.
  s->cs_seed           = lighting_load_shader("/assets/shaders/hrc/hrc_seed.c_shd");
  s->cs_trace          = lighting_load_shader("/assets/shaders/hrc/hrc_trace.c_shd");
  s->cs_extend         = lighting_load_shader("/assets/shaders/hrc/hrc_extend.c_shd");
  s->cs_merge          = lighting_load_shader("/assets/shaders/hrc/hrc_merge.c_shd");
  s->cs_copy           = lighting_load_shader("/assets/shaders/hrc/hrc_copy.c_shd");
  s->cs_composite      = lighting_load_shader("/assets/shaders/hrc/hrc_composite.c_shd");
  s->cs_minmax         = lighting_load_shader("/assets/shaders/hrc/hrc_minmax.c_shd");
  s->cs_prefilter      = lighting_load_shader("/assets/shaders/hrc/hrc_prefilter.c_shd");
  s->cs_prefilter_gauss = lighting_load_shader("/assets/shaders/hrc/hrc_prefilter_gauss.c_shd");

  s->initialized = true;
  log_info("lighting", "HRC initialized (world=%d, grid=%d)",
           LIGHTING_WORLD_SIZE, s->grid);
}

void lighting_shutdown(LightingState* s)
{
  if (!s->initialized) return;

  cf_destroy_canvas(s->emissivity);
  cf_destroy_canvas(s->absorption);
  for (int i = 0; i <= LIGHTING_MAX_N; i++) {
    cf_destroy_storage_buffer(s->vrays_rad[i]);
    cf_destroy_storage_buffer(s->vrays_trn[i]);
  }
  for (int i = 0; i < 2; i++)
    cf_destroy_storage_buffer(s->r_rad[i]);
  cf_destroy_storage_buffer(s->r_zero);
  for (int i = 0; i < 4; i++)
    cf_destroy_storage_buffer(s->frustum[i]);
  cf_destroy_canvas(s->fluence);
  cf_destroy_canvas(s->minmax);
  cf_destroy_canvas(s->emissivity_filtered);
  cf_destroy_canvas(s->absorption_filtered);
  cf_destroy_material(s->mat_trace);
  cf_destroy_material(s->mat_extend);
  cf_destroy_material(s->mat_merge);
  cf_destroy_material(s->mat_composite);
  cf_destroy_material(s->mat_copy);
  cf_destroy_material(s->mat_minmax);
  cf_destroy_material(s->mat_prefilter);
  cf_destroy_compute_shader(s->cs_seed);
  cf_destroy_compute_shader(s->cs_trace);
  cf_destroy_compute_shader(s->cs_extend);
  cf_destroy_compute_shader(s->cs_merge);
  cf_destroy_compute_shader(s->cs_copy);
  cf_destroy_compute_shader(s->cs_composite);
  cf_destroy_compute_shader(s->cs_minmax);
  cf_destroy_compute_shader(s->cs_prefilter);
  cf_destroy_compute_shader(s->cs_prefilter_gauss);

  s->initialized = false;
  log_info("lighting", "HRC shut down");
}

// =============================================================================
// Per-frame light list
// =============================================================================

void lighting_begin_frame(LightingState* s)
{
  s->light_count = 0;
}

void lighting_add_light(LightingState* s, Light light)
{
  if (!s->initialized || s->light_count >= LIGHTING_MAX_LIGHTS) return;
  s->lights[s->light_count++] = light;
}

// =============================================================================
// Absorption texture from LDtk int_grid
// =============================================================================

void lighting_build_absorption(LightingState* s, LdtkMap* map,
                                float camera_x, float camera_y)
{
  if (!s->initialized || !map->loaded) return;

  LdtkLevel* lvl   = &map->levels[map->active_level];
  int        gw    = lvl->grid_width;
  int        gh    = lvl->grid_height;
  float      cell_w = (float)lvl->width  / (float)gw;
  float      cell_h = (float)lvl->height / (float)gh;
  float      half_w = (float)lvl->width  * 0.5f;
  float      half_h = (float)lvl->height * 0.5f;

  // Clear absorption to black (not the game's sky-blue clear color).
  cf_clear_color(0, 0, 0, 0);

  // Use the same projection as draw_emissivity: game-canvas viewport centered
  // on the camera. Tiles are drawn in CF world coordinates (Y-up, origin at
  // level center).  Both passes share a single coordinate space so absorption
  // and emissivity stay in register as the camera scrolls.
  cf_draw_push();
  cf_draw_TSR_absolute(cf_v2(0, 0), cf_v2(1, 1), 0);
  float view_w = (float)CANVAS_WIDTH;
  float view_h = (float)CANVAS_HEIGHT;
  cf_draw_projection(cf_ortho_2d(camera_x, camera_y, view_w, view_h));

  CF_RenderState rs        = cf_render_state_defaults();
  rs.blend.pixel_format    = CF_PIXEL_FORMAT_R16G16B16A16_FLOAT;
  rs.blend.rgb_src_blend_factor   = CF_BLENDFACTOR_ONE;
  rs.blend.rgb_dst_blend_factor   = CF_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  rs.blend.rgb_op                 = CF_BLEND_OP_ADD;
  rs.blend.alpha_src_blend_factor = CF_BLENDFACTOR_ONE;
  rs.blend.alpha_dst_blend_factor = CF_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  rs.blend.alpha_op               = CF_BLEND_OP_ADD;
  cf_draw_push_render_state(rs);

  cf_clear_canvas(s->absorption);

  // Draw solid tiles as white rectangles in CF world space.
  // LDtk tile (gx, gy): top-left at (gx*cell_w, gy*cell_h) in LDtk coords.
  // CF conversion: x = ldtk_x - half_w,  y = half_h - ldtk_y  (Y-flip).
  cf_draw_push_color(cf_make_color_rgb_f(1.0f, 1.0f, 1.0f));
  for (int gy = 0; gy < gh; gy++) {
    for (int gx = 0; gx < gw; gx++) {
      if (lvl->int_grid[gy * gw + gx] == 0) continue;

      float x0 = (float)gx * cell_w - half_w;
      float x1 = x0 + cell_w;
      float y1 = half_h - (float)gy * cell_h;        // CF top of tile
      float y0 = y1 - cell_h;                          // CF bottom of tile

      cf_draw_quad_fill(cf_make_aabb(cf_v2(x0, y0), cf_v2(x1, y1)), 0);
    }
  }
  cf_draw_pop_color();

  cf_draw_pop_render_state();
  cf_render_to(s->absorption, true);
  cf_draw_pop();
}

// =============================================================================
// Emissivity pass (draw lights into emissivity canvas)
// =============================================================================

static void draw_emissivity(LightingState* s, float camera_x, float camera_y)
{
  // Project the same world area as the game canvas onto the HRC emissivity canvas.
  // cf_ortho_2d(x, y, scale_x, scale_y): centered at (x,y), visible area = scale_x × scale_y.
  cf_draw_push();
  cf_draw_TSR_absolute(cf_v2(0, 0), cf_v2(1, 1), 0);
  float view_w = (float)CANVAS_WIDTH;
  float view_h = (float)CANVAS_HEIGHT;
  cf_draw_projection(cf_ortho_2d(camera_x, camera_y, view_w, view_h));

  CF_RenderState rs        = cf_render_state_defaults();
  rs.blend.pixel_format    = CF_PIXEL_FORMAT_R16G16B16A16_FLOAT;
  rs.blend.rgb_src_blend_factor   = CF_BLENDFACTOR_ONE;
  rs.blend.rgb_dst_blend_factor   = CF_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  rs.blend.rgb_op                 = CF_BLEND_OP_ADD;
  rs.blend.alpha_src_blend_factor = CF_BLENDFACTOR_ONE;
  rs.blend.alpha_dst_blend_factor = CF_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  rs.blend.alpha_op               = CF_BLEND_OP_ADD;
  cf_draw_push_render_state(rs);

  // Inject ambient as a full-screen white quad scaled by ambient level.
  if (s->ambient > 0.001f) {
    float a = s->ambient;
    float half_vw = view_w * 0.5f;
    float half_vh = view_h * 0.5f;
    cf_draw_push_color(cf_make_color_rgba_f(a, a, a, 1.0f));
    cf_draw_quad_fill(cf_make_aabb(
        cf_v2(camera_x - half_vw, camera_y - half_vh),
        cf_v2(camera_x + half_vw, camera_y + half_vh)),
        0);
    cf_draw_pop_color();
  }

  // Draw each light as a filled shape into the emissivity canvas.
  for (int i = 0; i < s->light_count; i++) {
    Light* l = &s->lights[i];
    float  lr = l->r * l->intensity;
    float  lg = l->g * l->intensity;
    float  lb = l->b * l->intensity;
    cf_draw_push_color(cf_make_color_rgb_f(lr, lg, lb));

    if (l->cone >= 360.0f) {
      // Omnidirectional: full circle.
      cf_draw_circle_fill2(cf_v2(l->x, l->y), l->radius);
    } else {
      // Cone light: draw a triangle fan (pie slice).
      float dir_rad  = l->direction * (CF_PI / 180.0f);
      float half_rad = l->cone * 0.5f * (CF_PI / 180.0f);
      float start    = dir_rad - half_rad;
      int   segments = 16;
      float step     = (2.0f * half_rad) / (float)segments;
      CF_V2 center   = cf_v2(l->x, l->y);
      for (int seg = 0; seg < segments; seg++) {
        float a0 = start + step * (float)seg;
        float a1 = start + step * (float)(seg + 1);
        CF_V2 p0 = cf_v2(l->x + cosf(a0) * l->radius,
                          l->y + sinf(a0) * l->radius);
        CF_V2 p1 = cf_v2(l->x + cosf(a1) * l->radius,
                          l->y + sinf(a1) * l->radius);
        cf_draw_tri_fill(center, p0, p1, 0);
      }
    }

    cf_draw_pop_color();
  }

  cf_draw_pop_render_state();
  cf_render_to(s->emissivity, true);
  cf_draw_pop();
}

// =============================================================================
// HRC compute pipeline (ported from hrc_compute() in hrc.c)
// =============================================================================

void lighting_compute(LightingState* s, float camera_x, float camera_y)
{
  if (!s->initialized) return;

  draw_emissivity(s, camera_x, camera_y);

  CF_Texture emiss_tex   = cf_canvas_get_target(s->emissivity);
  CF_Texture absrp_tex   = cf_canvas_get_target(s->absorption);
  CF_Texture fluence_tex = cf_canvas_get_target(s->fluence);
  CF_Texture minmax_tex  = cf_canvas_get_target(s->minmax);

  int dim   = s->grid;
  int ncasc = s->n;
  int world = LIGHTING_WORLD_SIZE;

  // Textures fed to trace shader (may be swapped to prefiltered versions).
  CF_Texture trace_emiss = emiss_tex;
  CF_Texture trace_absrp = absrp_tex;

  // Mipmap prefilter: generate hardware mipmaps for textureLod sampling.
  float mip_level      = 0.0f;
  float absrp_mip_level = 0.0f;
  if ((s->prefilter_mode == 2 || s->prefilter_mode == 4) && dim < LIGHTING_WORLD_SIZE) {
    cf_generate_mipmaps(emiss_tex);
    mip_level = log2f((float)LIGHTING_WORLD_SIZE / (float)dim);
    if (s->prefilter_mode == 2) {
      cf_generate_mipmaps(absrp_tex);
      absrp_mip_level = mip_level;
    }
  }

  // Box/Gauss prefilter: downsample to grid resolution.
  if ((s->prefilter_mode == 1 || s->prefilter_mode == 3 || s->prefilter_mode == 4)
      && dim < LIGHTING_WORLD_SIZE) {
    CF_Texture emiss_filt_tex = cf_canvas_get_target(s->emissivity_filtered);
    CF_Texture absrp_filt_tex = cf_canvas_get_target(s->absorption_filtered);

    cf_material_set_texture_cs(s->mat_prefilter, "u_emissivity", emiss_tex);
    cf_material_set_texture_cs(s->mat_prefilter, "u_absorption", absrp_tex);
    cf_material_set_uniform_cs(s->mat_prefilter, "u_world_size", &world, CF_UNIFORM_TYPE_INT, 1);
    cf_material_set_uniform_cs(s->mat_prefilter, "u_grid_size",  &dim,   CF_UNIFORM_TYPE_INT, 1);

    CF_ComputeDispatch d = cf_compute_dispatch_defaults(
        lighting_div_ceil(dim, HRC_WG), lighting_div_ceil(dim, HRC_WG), 1);
    CF_Texture rw_tex[2] = { emiss_filt_tex, absrp_filt_tex };
    d.rw_textures      = rw_tex;
    d.rw_texture_count = 2;
    CF_ComputeShader cs = (s->prefilter_mode == 3)
        ? s->cs_prefilter_gauss : s->cs_prefilter;
    cf_dispatch_compute(cs, s->mat_prefilter, d);

    if (s->prefilter_mode == 4) {
      trace_absrp = absrp_filt_tex;
    } else {
      trace_emiss = emiss_filt_tex;
      trace_absrp = absrp_filt_tex;
    }
  }

  cf_material_set_uniform_cs(s->mat_trace, "u_mip_level",      &mip_level,       CF_UNIFORM_TYPE_FLOAT, 1);
  cf_material_set_uniform_cs(s->mat_trace, "u_absrp_mip_level", &absrp_mip_level, CF_UNIFORM_TYPE_FLOAT, 1);

  for (int j = 0; j < 4; j++) {
    // Seed T_0 (when trace_levels == 0).
    if (s->trace_levels == 0) {
      int params[4] = { 0, j, dim, s->vrays_w[0] };
      cf_material_set_texture_cs(s->mat_trace, "u_emissivity", trace_emiss);
      cf_material_set_texture_cs(s->mat_trace, "u_absorption", trace_absrp);
      cf_material_set_uniform_cs(s->mat_trace, "u_cascade",    params + 0, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_rotate",     params + 1, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_world_size", params + 2, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_curr_w",     params + 3, CF_UNIFORM_TYPE_INT, 1);

      CF_ComputeDispatch d = cf_compute_dispatch_defaults(
          lighting_div_ceil(s->vrays_w[0], HRC_WG), lighting_div_ceil(dim, HRC_WG), 1);
      CF_StorageBuffer rw[2] = { s->vrays_rad[0], s->vrays_trn[0] };
      d.rw_buffers      = rw;
      d.rw_buffer_count = 2;
      cf_dispatch_compute(s->cs_seed, s->mat_trace, d);
    }

    // Direct trace T_0..T_{trace_levels-1}.
    for (int i = 0; i < s->trace_levels; i++) {
      int params[5] = { i, j, dim, s->vrays_w[i], s->blend_boundary };
      cf_material_set_texture_cs(s->mat_trace, "u_emissivity",    trace_emiss);
      cf_material_set_texture_cs(s->mat_trace, "u_absorption",    trace_absrp);
      cf_material_set_uniform_cs(s->mat_trace, "u_cascade",       params + 0, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_rotate",        params + 1, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_world_size",    params + 2, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_curr_w",        params + 3, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_blend_boundary", params + 4, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_trace, "u_blend_width",   &s->blend_width, CF_UNIFORM_TYPE_FLOAT, 1);

      CF_ComputeDispatch d = cf_compute_dispatch_defaults(
          lighting_div_ceil(s->vrays_w[i], HRC_WG), lighting_div_ceil(dim, HRC_WG), 1);
      CF_StorageBuffer rw[2] = { s->vrays_rad[i], s->vrays_trn[i] };
      d.rw_buffers      = rw;
      d.rw_buffer_count = 2;
      cf_dispatch_compute(s->cs_trace, s->mat_trace, d);
    }

    // Extend remaining cascade levels.
    int ext_start = s->trace_levels > 0 ? s->trace_levels : 1;
    for (int i = ext_start; i <= ncasc; i++) {
      int params[4] = { i, dim, s->vrays_w[i - 1], s->vrays_w[i] };
      cf_material_set_uniform_cs(s->mat_extend, "u_cascade",    params + 0, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_extend, "u_world_size", params + 1, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_extend, "u_prev_w",     params + 2, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_extend, "u_curr_w",     params + 3, CF_UNIFORM_TYPE_INT, 1);

      CF_ComputeDispatch d = cf_compute_dispatch_defaults(
          lighting_div_ceil(s->vrays_w[i], HRC_WG), lighting_div_ceil(dim, HRC_WG), 1);
      CF_StorageBuffer ro[2] = { s->vrays_rad[i - 1], s->vrays_trn[i - 1] };
      d.ro_buffers      = ro;
      d.ro_buffer_count = 2;
      CF_StorageBuffer rw[2] = { s->vrays_rad[i], s->vrays_trn[i] };
      d.rw_buffers      = rw;
      d.rw_buffer_count = 2;
      cf_dispatch_compute(s->cs_extend, s->mat_extend, d);
    }

    // Merge R_{ncasc-1} down to R_0.
    int r_ping = 0;
    for (int i = ncasc - 1; i >= 0; i--) {
      int params[6] = { i, dim, s->vrays_w[i], s->vrays_w[i + 1], dim, dim };
      cf_material_set_uniform_cs(s->mat_merge, "u_cascade",    params + 0, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_merge, "u_world_size", params + 1, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_merge, "u_t_curr_w",   params + 2, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_merge, "u_t_next_w",   params + 3, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_merge, "u_r_prev_w",   params + 4, CF_UNIFORM_TYPE_INT, 1);
      cf_material_set_uniform_cs(s->mat_merge, "u_r_curr_w",   params + 5, CF_UNIFORM_TYPE_INT, 1);

      CF_ComputeDispatch d = cf_compute_dispatch_defaults(
          lighting_div_ceil(dim, HRC_WG), lighting_div_ceil(dim, HRC_WG), 1);
      CF_StorageBuffer r_prev = (i == ncasc - 1) ? s->r_zero : s->r_rad[1 - r_ping];
      CF_StorageBuffer ro[5] = {
        s->vrays_rad[i],   s->vrays_trn[i],
        s->vrays_rad[i+1], s->vrays_trn[i+1],
        r_prev
      };
      d.ro_buffers      = ro;
      d.ro_buffer_count = 5;
      CF_StorageBuffer rw[1] = { s->r_rad[r_ping] };
      d.rw_buffers      = rw;
      d.rw_buffer_count = 1;
      cf_dispatch_compute(s->cs_merge, s->mat_merge, d);
      r_ping = 1 - r_ping;
    }

    // Copy R_0 → frustum[j].
    {
      int count = dim * dim;
      cf_material_set_uniform_cs(s->mat_copy, "u_count", &count, CF_UNIFORM_TYPE_INT, 1);

      CF_ComputeDispatch d = cf_compute_dispatch_defaults(
          lighting_div_ceil(count, 256), 1, 1);
      CF_StorageBuffer ro[1] = { s->r_rad[1 - r_ping] };
      d.ro_buffers      = ro;
      d.ro_buffer_count = 1;
      CF_StorageBuffer rw[1] = { s->frustum[j] };
      d.rw_buffers      = rw;
      d.rw_buffer_count = 1;
      cf_dispatch_compute(s->cs_copy, s->mat_copy, d);
    }
  }

  // Minmax absorption pass (for bilinear upscale mode).
  if (s->upscale_mode == 1) {
    cf_material_set_texture_cs(s->mat_minmax, "u_absorption", absrp_tex);
    cf_material_set_uniform_cs(s->mat_minmax, "u_world_size", &world, CF_UNIFORM_TYPE_INT, 1);
    cf_material_set_uniform_cs(s->mat_minmax, "u_grid_size",  &dim,   CF_UNIFORM_TYPE_INT, 1);

    CF_ComputeDispatch d = cf_compute_dispatch_defaults(
        lighting_div_ceil(world, HRC_WG), lighting_div_ceil(world, HRC_WG), 1);
    CF_Texture rw_tex[1] = { minmax_tex };
    d.rw_textures      = rw_tex;
    d.rw_texture_count = 1;
    cf_dispatch_compute(s->cs_minmax, s->mat_minmax, d);
  }

  // Composite: c-1 gathering, sum 4 quadrants, cross blur → fluence.
  {
    float abs_thresh = 0.1f;
    int   debug      = s->debug_mode <= 5 ? s->debug_mode : 0;
    cf_material_set_texture_cs(s->mat_composite, "u_emissivity",   emiss_tex);
    cf_material_set_texture_cs(s->mat_composite, "u_absorption",   absrp_tex);
    cf_material_set_texture_cs(s->mat_composite, "u_minmax",       minmax_tex);
    cf_material_set_uniform_cs(s->mat_composite, "u_world_size",   &world,        CF_UNIFORM_TYPE_INT,   1);
    cf_material_set_uniform_cs(s->mat_composite, "u_grid_size",    &dim,          CF_UNIFORM_TYPE_INT,   1);
    cf_material_set_uniform_cs(s->mat_composite, "u_abs_threshold", &abs_thresh,  CF_UNIFORM_TYPE_FLOAT, 1);
    cf_material_set_uniform_cs(s->mat_composite, "u_debug_mode",   &debug,        CF_UNIFORM_TYPE_INT,   1);
    cf_material_set_uniform_cs(s->mat_composite, "u_cminus1",      &s->cminus1,   CF_UNIFORM_TYPE_INT,   1);
    cf_material_set_uniform_cs(s->mat_composite, "u_upscale_mode", &s->upscale_mode, CF_UNIFORM_TYPE_INT, 1);

    CF_ComputeDispatch d = cf_compute_dispatch_defaults(
        lighting_div_ceil(world, HRC_WG), lighting_div_ceil(world, HRC_WG), 1);
    CF_StorageBuffer ro[4] = {
      s->frustum[0], s->frustum[1],
      s->frustum[2], s->frustum[3]
    };
    d.ro_buffers      = ro;
    d.ro_buffer_count = 4;
    CF_Texture rw_tex[1] = { fluence_tex };
    d.rw_textures      = rw_tex;
    d.rw_texture_count = 1;
    cf_dispatch_compute(s->cs_composite, s->mat_composite, d);
  }
}

// =============================================================================
// Fluence canvas accessor
// =============================================================================

CF_Canvas lighting_fluence_canvas(LightingState* s)
{
  if (s->debug_mode == 6) return s->emissivity;
  if (s->debug_mode == 7) return s->absorption;
  return s->fluence;
}
