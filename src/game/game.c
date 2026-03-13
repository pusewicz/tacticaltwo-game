#include "game.h"

#include <cute_alloc.h>
#include <cute_app.h>
#include <cute_c_runtime.h>
#include <cute_defines.h>
#include <cute_draw.h>
#include <cute_graphics.h>
#include <cute_input.h>
#include <cute_math.h>
#include <cute_sprite.h>
#include <cute_time.h>
#include <dcimgui.h>
#include <stdlib.h>

#include "../config/config.h"
#include "../engine/game_state.h"
#include "../engine/log.h"
#include "../engine/platform.h"
#include "ldtk.h"
#include "lighting.h"
#include "rain.h"
#include "world.h"

GameState* state = nullptr;

// Handles hot-reload for all game shaders (overrides rain.c's callback)
static void on_shader_changed([[maybe_unused]] const char* path,
                               [[maybe_unused]] void* udata) {
  if (state->world.rain.initialized) {
    cf_shader_reload(&state->world.rain.shader);
  }
}

static CF_V2 calculate_dest_size(CF_V2 game, CF_V2 window) {
  float game_aspect   = game.x / game.y;
  float window_aspect = window.x / window.y;
  float dest_w;
  float dest_h;

  if (window_aspect > game_aspect) {
    // Window is wider - pillarbox (black bars on sides).
    dest_h = window.y;
    dest_w = dest_h * game_aspect;
  } else {
    // Window is taller - letterbox (black bars on top/bottom).
    dest_w = window.x;
    dest_h = dest_w / game_aspect;
  }

  return cf_v2(dest_w, dest_h);
}

// Ensure composite canvas exists and matches current window dimensions.
// Returns false if window has zero size (minimized).
static bool ensure_composite_canvas(void) {
  int w = cf_app_get_width();
  int h = cf_app_get_height();
  if (w <= 0 || h <= 0) {
    return false;
  }

  if (state->composite_w == w && state->composite_h == h) {
    return true;
  }

  // Destroy old canvas if it existed.
  if (state->composite_w > 0) {
    cf_destroy_canvas(state->composite_canvas);
  }

  state->composite_canvas = cf_make_canvas(cf_canvas_defaults(w, h));
  state->composite_w = w;
  state->composite_h = h;
  return true;
}

void game_init(Platform* platform) {
  state = calloc(1, sizeof(GameState));
  CF_ASSERT(state != nullptr);

  state->platform       = platform;
  state->scratch_arena  = malloc(sizeof(CF_Arena));
  *state->scratch_arena = cf_make_arena(_Alignof(void*), CF_MB * 4);
  state->canvas =
      cf_make_canvas(cf_canvas_defaults(CANVAS_WIDTH, CANVAS_HEIGHT));

  // Set up projection for the game canvas
  cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH * CANVAS_SCALE,
                                 CANVAS_HEIGHT * CANVAS_SCALE));

  cf_shader_directory("/assets/shaders");

  init_world();
  // Global shader hot-reload callback (supersedes rain.c's callback)
  cf_shader_on_changed(on_shader_changed, nullptr);

  cf_app_init_imgui();
}

bool game_update(void) {
  cf_arena_reset(state->scratch_arena);

  if (cf_key_just_pressed(CF_KEY_G)) {
    state->debug_mode = !state->debug_mode;
  }

  update_world(CF_DELTA_TIME);

  if (state->debug_mode) {
    // Create a full-viewport dockspace.
    ImGui_DockSpaceOverViewport();

    // --- Game window: display composite canvas texture ---
    ImGui_Begin("Game", nullptr, 0);
    if (state->composite_w > 0) {
      ImVec2 avail = ImGui_GetContentRegionAvail();
      // Maintain game aspect ratio within available space.
      float game_aspect = (float)CANVAS_WIDTH / (float)CANVAS_HEIGHT;
      float w = avail.x;
      float h = w / game_aspect;
      if (h > avail.y) {
        h = avail.y;
        w = h * game_aspect;
      }
      CF_Texture tex = cf_canvas_get_target(state->composite_canvas);
      uint64_t tex_id = cf_texture_handle(tex);
      ImTextureRef ref = { ._TexData = nullptr, ._TexID = tex_id };
      ImGui_Image(ref, (ImVec2){w, h});
    }
    ImGui_End();

    // --- Debug window: all debug controls ---
    ImGui_Begin("Debug", &state->debug_mode, 0);

    int p = state->world.player;
    if (p != ENTITY_NONE) {
      Entity* player = &state->world.entities[p];

      ImGui_SeparatorText("Collider");
      ImGui_SliderFloat("half_w", &player->collider.half_size.x, 1.0f, 32.0f);
      ImGui_SliderFloat("half_h", &player->collider.half_size.y, 1.0f, 48.0f);
      ImGui_SliderFloat("offset_x", &player->collider.offset.x, -48.0f, 48.0f);
      ImGui_SliderFloat("offset_y", &player->collider.offset.y, -48.0f, 48.0f);
      ImGui_Text("grounded: %s", player->collider.grounded ? "true" : "false");

      ImGui_SeparatorText("Transform");
      ImGui_Text("pos: %.1f, %.1f", (double)player->transform.position.x,
                 (double)player->transform.position.y);
      ImGui_Text("vel: %.1f, %.1f", (double)player->velocity.value.x,
                 (double)player->velocity.value.y);
    }

    LdtkMap* map = &state->world.map;
    if (map->loaded) {
      LdtkLevel* lvl = &map->levels[map->active_level];
      ImGui_SeparatorText("Level");
      ImGui_Text("name: %s", lvl->identifier);
      ImGui_Text("pos: %d, %d  size: %dx%d", lvl->x, lvl->y, lvl->width,
                 lvl->height);
      ImGui_Text("grid: %dx%d", lvl->grid_width, lvl->grid_height);
    }

    ImGui_SeparatorText("Rain");
    ImGui_Checkbox("rain enabled", &state->world.rain.enabled);
    ImGui_SliderFloat("intensity", &state->world.rain.intensity, 0.0f, 3.0f);
    ImGui_SliderFloat("density", &state->world.rain.density, 0.0f, 3.0f);
    ImGui_SliderFloat("alpha", &state->world.rain.alpha, 0.0f, 1.0f);
    ImGui_SliderFloat("speed", &state->world.rain.speed, 0.1f, 15.0f);
    ImGui_SliderFloat("wind", &state->world.rain.wind, -1.0f, 1.0f);

    ImGui_SeparatorText("Rain Splash");
    ImGui_SliderFloat("cell size", &state->world.rain.splash_cell_size, 4.0f, 30.0f);
    ImGui_SliderFloat("rate", &state->world.rain.splash_rate, 1.0f, 20.0f);
    ImGui_SliderFloat("life", &state->world.rain.splash_life, 0.05f, 3.0f);
    ImGui_SliderFloat("splash speed", &state->world.rain.splash_speed, 2.0f, 60.0f);
    ImGui_SliderFloat("gravity", &state->world.rain.splash_gravity, 5.0f, 80.0f);

    ImGui_SeparatorText("Lighting (HRC)");
    LightingState* lt = &state->world.lighting;
    ImGui_SliderFloat("ambient", &lt->ambient, 0.0f, 1.0f);
    int grid_sizes[] = {64, 128, 256, 512};
    int grid_idx = 0;
    for (int i = 0; i < 4; i++) {
      if (lt->grid == grid_sizes[i]) { grid_idx = i; break; }
    }
    if (ImGui_Combo("grid", &grid_idx, "64\000128\000256\000512\000\000")) {
      lighting_set_grid(lt, grid_sizes[grid_idx]);
    }
    ImGui_SliderInt("debug_mode", &lt->debug_mode, 0, 7);
    ImGui_Checkbox("cminus1", (bool*)&lt->cminus1);
    ImGui_SliderInt("trace_levels", &lt->trace_levels, 0, lt->n + 1);

    for (int i = 0; i < state->world.lights_static_count; i++) {
      Light* l = &state->world.lights_static[i];
      ImGui_PushIDInt(i);
      char label[32];
      snprintf(label, sizeof(label), "Light %d", i);
      if (ImGui_TreeNode(label)) {
        ImGui_SliderFloat("x", &l->x, -400.0f, 400.0f);
        ImGui_SliderFloat("y", &l->y, -200.0f, 200.0f);
        float color[3] = {l->r, l->g, l->b};
        if (ImGui_ColorEdit3("color", color, 0)) {
          l->r = color[0];
          l->g = color[1];
          l->b = color[2];
        }
        ImGui_SliderFloat("intensity", &l->intensity, 0.0f, 10.0f);
        ImGui_SliderFloat("radius", &l->radius, 1.0f, 200.0f);
        ImGui_SliderFloat("direction", &l->direction, 0.0f, 360.0f);
        ImGui_SliderFloat("cone", &l->cone, 10.0f, 360.0f);
        ImGui_TreePop();
      }
      ImGui_PopID();
    }

    ImGui_SeparatorText("Muzzle Flash");
    MuzzleFlash* mf = &state->world.muzzle_flash;
    if (mf->duration == 0.0f) {
      mf->duration       = 0.12f;
      mf->peak_intensity = 1.5f;
      mf->radius_min     = 18.0f;
      mf->radius_max     = 45.0f;
    }
    ImGui_SliderFloat("mf_duration", &mf->duration, 0.02f, 1.0f);
    ImGui_SliderFloat("mf_intensity", &mf->peak_intensity, 0.1f, 10.0f);
    ImGui_SliderFloat("mf_radius_min", &mf->radius_min, 1.0f, 100.0f);
    ImGui_SliderFloat("mf_radius_max", &mf->radius_max, 1.0f, 200.0f);
    ImGui_Text("active: %s  timer: %.3f", mf->active ? "true" : "false",
               (double)mf->timer);
    if (ImGui_Button("Test Fire")) {
      int pid = state->world.player;
      if (pid != ENTITY_NONE) {
        Entity* pe = &state->world.entities[pid];
        world_trigger_muzzle_flash(pe->transform.position.x,
                                   pe->transform.position.y);
      }
    }

    ImGui_End();
  }

  return true;
}

void game_render(void) {
  cf_draw_push_filter(CF_DRAW_FILTER_NEAREST);

  // PASS 1: Render game scene to the game canvas.
  {
    cf_clear_color(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f);
    cf_clear_canvas(state->canvas);
    render_world();
    cf_render_to(state->canvas, true);
  }

  // PASS 2: HRC lighting — collect lights, run compute pipeline.
  {
    LightingState* lt = &state->world.lighting;
    CF_V2 cam = state->world.camera;

    // Rebuild absorption each frame with current camera so it stays registered
    // with the emissivity pass (both use the same camera-relative projection).
    lighting_build_absorption(lt, &state->world.map, cam.x, cam.y);

    lighting_begin_frame(lt);

    // Re-add static lights from LDtk each frame.
    for (int i = 0; i < state->world.lights_static_count; i++) {
      Light* l = &state->world.lights_static[i];
      lighting_add_light(lt, *l);
    }

    // Muzzle flash dynamic light.
    MuzzleFlash* mf = &state->world.muzzle_flash;
    if (mf->active && mf->timer > 0.0f && mf->duration > 0.0f) {
      float t     = 1.0f - (mf->timer / mf->duration);
      float inv_t = 1.0f - t;
      float range = mf->radius_max - mf->radius_min;
      lighting_add_light(lt, (Light){
          .x         = mf->x,
          .y         = mf->y,
          .r         = 1.0f,
          .g         = 0.9f - 0.4f * t,
          .b         = 0.5f - 0.4f * t,
          .intensity = mf->peak_intensity * inv_t * inv_t,
          .radius    = mf->radius_min + range * t,
          .direction = 0.0f,
          .cone      = 360.0f,
      });
    }

    lighting_compute(lt, cam.x, cam.y);
  }

  // PASS 3: Composite game + lighting.
  if (state->debug_mode && ensure_composite_canvas()) {
    // Debug mode: render to composite canvas (displayed via ImGui Image).
    int w = state->composite_w;
    int h = state->composite_h;

    cf_clear_color(0, 0, 0, 1.0f);
    cf_clear_canvas(state->composite_canvas);

    CF_V2 dest = calculate_dest_size(cf_v2(CANVAS_WIDTH, CANVAS_HEIGHT),
                                     cf_v2((float)w, (float)h));
    cf_draw_projection(cf_ortho_2d(0, 0, (float)w, (float)h));

    cf_draw_canvas(state->canvas, cf_v2(0, 0), dest);

    CF_RenderState rs             = cf_render_state_defaults();
    rs.blend.rgb_src_blend_factor = CF_BLENDFACTOR_DST_COLOR;
    rs.blend.rgb_dst_blend_factor = CF_BLENDFACTOR_ZERO;
    rs.blend.rgb_op               = CF_BLEND_OP_ADD;
    rs.blend.alpha_src_blend_factor = CF_BLENDFACTOR_ZERO;
    rs.blend.alpha_dst_blend_factor = CF_BLENDFACTOR_ONE;
    rs.blend.alpha_op               = CF_BLEND_OP_ADD;
    cf_draw_push_render_state(rs);
    cf_draw_canvas(lighting_fluence_canvas(&state->world.lighting),
                   cf_v2(0, 0), dest);
    cf_draw_pop_render_state();

    cf_render_to(state->composite_canvas, true);

    cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
  } else {
    // Normal mode: render to screen (existing behavior).
    int window_w = cf_app_get_width();
    int window_h = cf_app_get_height();

    cf_clear_color(0, 0, 0, 1.0f);

    cf_app_set_canvas_size(window_w, window_h);
    CF_V2 dest = calculate_dest_size(cf_v2(CANVAS_WIDTH, CANVAS_HEIGHT),
                                     cf_v2((float)window_w, (float)window_h));
    cf_draw_projection(cf_ortho_2d(0, 0, (float)window_w, (float)window_h));

    cf_draw_canvas(state->canvas, cf_v2(0, 0), dest);

    CF_RenderState rs             = cf_render_state_defaults();
    rs.blend.rgb_src_blend_factor = CF_BLENDFACTOR_DST_COLOR;
    rs.blend.rgb_dst_blend_factor = CF_BLENDFACTOR_ZERO;
    rs.blend.rgb_op               = CF_BLEND_OP_ADD;
    rs.blend.alpha_src_blend_factor = CF_BLENDFACTOR_ZERO;
    rs.blend.alpha_dst_blend_factor = CF_BLENDFACTOR_ONE;
    rs.blend.alpha_op               = CF_BLEND_OP_ADD;
    cf_draw_push_render_state(rs);
    cf_draw_canvas(lighting_fluence_canvas(&state->world.lighting),
                   cf_v2(0, 0), dest);
    cf_draw_pop_render_state();

    cf_draw_projection(cf_ortho_2d(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT));
  }

  cf_draw_pop_filter();
}

void game_shutdown(void) {
  shutdown_world();
  if (state->composite_w > 0) {
    cf_destroy_canvas(state->composite_canvas);
  }
  cf_destroy_canvas(state->canvas);
  cf_destroy_arena(state->scratch_arena);
  free(state->scratch_arena);
  free(state);
}

void* game_state(void) { return state; }

void game_hot_reload(void* game_state) {
  state = (GameState*)game_state;
  world_hot_reload();
}
