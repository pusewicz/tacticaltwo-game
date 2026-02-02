#include <cute_draw.h>

#include "../engine/platform.h"
#include "../platform/platform_cute.h"
#ifndef ENGINE_HOT_RELOADING
#include "game/game.h"
#endif

#include <SDL3/SDL_timer.h>
#include <cute_app.h>
#include <cute_color.h>
#include <cute_math.h>
#include <cute_symbol.h>
#include <cute_time.h>
#include <stddef.h>
#include <stdio.h>

#include "../engine/log.h"

// Counter for number of times the game has been reloaded
static int reloaded_counter = 0;

static void on_update(void* udata) {
#ifdef ENGINE_HOT_RELOADING
  GameLibrary* game_library = (GameLibrary*)udata;

  if (platform_game_library_has_changed(game_library)) {
    log_info("main", "Game library updated, reloading!");

    void* game_state = game_library->state();
    platform_unload_game_library(game_library);

    SDL_Delay(50);
    GameLibrary new_game_library = platform_load_game_library();
    if (new_game_library.ok) {
      ++reloaded_counter;

      log_info("main", "Game reloaded successfully! (total reloads: %d)",
               reloaded_counter);

      *game_library = new_game_library;
      game_library->hot_reload(game_state);
    }
  }

  if (!game_library->update()) {
    cf_app_signal_shutdown();
  }
#else
  if (!game_update()) {
    cf_app_signal_shutdown();
  }
#endif
}

int main(int argc, char* argv[]) {
  platform_init(argc, argv);

  Platform platform = {
      .get_system_page_size = platform_get_page_size,
  };

#ifdef ENGINE_HOT_RELOADING
  GameLibrary game_library = platform_load_game_library();
  game_library.init(&platform);

  cf_set_update_udata(&game_library);
#else
  game_init(&platform);
#endif

  while (cf_app_is_running()) {
    cf_app_update(on_update);

    platform_begin_frame();
#ifdef ENGINE_HOT_RELOADING
    game_library.render();
#else
    game_render();
#endif

    platform_end_frame();
  }

#ifdef ENGINE_HOT_RELOADING
  game_library.shutdown();
  platform_unload_game_library(&game_library);
#else
  game_shutdown();
#endif

  platform_shutdown();

  return 0;
}
