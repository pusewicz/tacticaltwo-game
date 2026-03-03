#pragma once

#include <mruby.h>
#include <stdbool.h>

#include "../world.h" // for ecs_entity_t

bool scripting_init(void);
void scripting_shutdown(void);
void scripting_hot_reload(void);
void scripting_update(float dt);
bool scripting_load_file(const char* path);
// Resume the script's fiber for one frame. Sets script->active = false when the fiber dies.
void scripting_resume_fiber(C_Script* script);

// Load path, expect the last expression to be a Fiber, attach it to entity.
bool scripting_attach(ecs_entity_t entity, const char* path);
