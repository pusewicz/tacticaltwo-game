#include "platform_cute.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>
#include <cute_app.h>
#include <cute_defines.h>
#include <cute_file_system.h>
#include <cute_graphics.h>
#include <cute_result.h>
#include <cute_symbol.h>
#include <cute_time.h>
#include <stddef.h>

#include "../engine/log.h"
#include "config.h"
#if __has_include(<_abort.h>)
#include <_abort.h>
#elif __has_include(<stdlib.h>)
#include <stdlib.h>
#endif

#define MAX_PATH_LENGTH 1024

static char game_library_path[MAX_PATH_LENGTH] = {0};
static SDL_PathInfo path_info;

#ifdef SDL_PLATFORM_WIN32
static char game_library_copy_path[MAX_PATH_LENGTH] = {0};
#endif

void platform_init(int argc [[maybe_unused]], char* argv[]) {
  log_init();

  log_info("platform", "Initializing platform...");

  SDL_SetAppMetadata(GAME_NAME, GAME_VERSION, GAME_APP_ID);
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

  int options = CF_APP_OPTIONS_RESIZABLE_BIT;
  CF_Result result =
      cf_make_app(GAME_NAME, 0, 0, 0, CANVAS_WIDTH * CANVAS_SCALE,
                  CANVAS_HEIGHT * CANVAS_SCALE, options, argv[0]);

  cf_set_fixed_timestep(60);
  cf_set_target_framerate(60);
  cf_app_set_vsync(false);

  if (cf_is_error(result)) {
    log_fatal("platform", "Failed to create app: %s", result.details);
    abort();
  }

  // Mount assets directory
#ifdef ASSETS_PATH
  // Development: mount source assets directly
  log_debug("platform", "Mounting assets from: %s", ASSETS_PATH);
  cf_fs_mount(ASSETS_PATH, "/assets", true);
#else
  // Release: assets are next to executable
  const char* base = cf_fs_get_base_directory();
  char assets_path[512];
  snprintf(assets_path, sizeof(assets_path), "%sassets", base);
  log_debug("platform", "Mounting assets from: %s", assets_path);
  cf_fs_mount(assets_path, "/assets", true);
#endif

  log_debug("platform", "Base directory: %s", cf_fs_get_base_directory());
  log_debug("platform", "Working directory: %s", cf_fs_get_working_directory());
  log_debug("platform", "Platform initialized!");
}

void platform_shutdown(void) { cf_destroy_app(); }

int platform_get_page_size(void) {
  return 4096; // TODO: Use SDL_GetSystemPageSize() after SDL 3.4.0;
}

GameLibrary platform_load_game_library(void) {
  GameLibrary game_library = {0};

  const char* base_path = SDL_GetBasePath();
  if (!base_path) {
    log_error("platform", "Failed to get base path: %s", SDL_GetError());
    return game_library;
  }

  const char* game_library_name =
      GAME_LIB_PREFIX GAME_LIB_BASENAME GAME_LIB_SUFFIX;
#ifdef SDL_PLATFORM_WIN32
  const char* game_library_copy_name =
      GAME_LIB_PREFIX GAME_LIB_BASENAME "_copy" GAME_LIB_SUFFIX;
  SDL_snprintf(game_library_copy_path, CF_ARRAY_SIZE(game_library_copy_path),
               "%s%s", base_path, game_library_copy_name);
#endif

  SDL_snprintf(game_library_path, CF_ARRAY_SIZE(game_library_path), "%s%s",
               base_path, game_library_name);

  if (!SDL_GetPathInfo(game_library_path, &path_info)) {
    log_error("platform", "Failed to get path info (%s): %s", game_library_path,
              SDL_GetError());
    return game_library;
  }

#ifdef SDL_PLATFORM_WIN32
  if (!SDL_CopyFile(game_library_path, game_library_copy_path)) {
    log_error("platform", "Failed to copy library: %s", SDL_GetError());
    return game_library;
  }
#endif

#ifdef SDL_PLATFORM_WIN32
  game_library.path = game_library_copy_path;
#else
  game_library.path = game_library_path;
#endif

  game_library.library = cf_load_shared_library(game_library.path);
  if (!game_library.library) {
    log_error("platform", "Failed to load library: %s", SDL_GetError());
    return game_library;
  }

  game_library.init =
      (GameInitFunction)cf_load_function(game_library.library, "game_init");
  if (!game_library.init) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.update =
      (GameUpdateFunction)cf_load_function(game_library.library, "game_update");
  if (!game_library.update) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.render =
      (GameRenderFunction)cf_load_function(game_library.library, "game_render");
  if (!game_library.render) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.shutdown = (GameShutdownFunction)cf_load_function(
      game_library.library, "game_shutdown");
  if (!game_library.shutdown) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.state =
      (GameStateFunction)cf_load_function(game_library.library, "game_state");
  if (!game_library.state) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.hot_reload = (GameHotReloadFunction)cf_load_function(
      game_library.library, "game_hot_reload");
  if (!game_library.hot_reload) {
    log_error("platform", "Failed to load function: %s", SDL_GetError());
    return game_library;
  }

  game_library.ok = true;

  return game_library;
}

void platform_begin_frame(void) {}
void platform_end_frame(void) { cf_app_draw_onto_screen(true); }

void platform_unload_game_library(GameLibrary* game_library) {
  cf_unload_shared_library(game_library->library);
  game_library->hot_reload = nullptr;
  game_library->state      = nullptr;
  game_library->shutdown   = nullptr;
  game_library->render     = nullptr;
  game_library->update     = nullptr;
  game_library->init       = nullptr;
  game_library->library    = nullptr;
  game_library->ok         = false;
}

bool platform_game_library_has_changed(GameLibrary* game_library) {
  SDL_PathInfo new_path_info;
  if (!SDL_GetPathInfo(game_library->path, &new_path_info)) {
    log_error("platform", "Failed to get path info: %s", SDL_GetError());
    return false;
  }
  return new_path_info.modify_time != path_info.modify_time;
}
