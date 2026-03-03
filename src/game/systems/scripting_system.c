// scripting_system.c - ECS scripting system
//
// Resumes per-entity Ruby fibers each frame.

#include <stddef.h>

#include "../../engine/game_state.h"
#include "../scripting/scripting.h"
#include "systems.h"

ecs_ret_t sys_scripting([[maybe_unused]] ecs_t* ecs, ecs_entity_t* entities,
                        size_t count, [[maybe_unused]] void* udata) {
  for (size_t i = 0; i < count; i++) {
    auto script = ECS_GET(entities[i], C_Script);
    if (!script->active) {
      continue;
    }
    scripting_resume_fiber(script);
  }
  return 0;
}
