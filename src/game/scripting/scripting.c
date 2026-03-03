// scripting.c - mruby VM lifecycle and script loading

#include "scripting.h"

#include <mruby.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/gc.h>
#include <mruby/string.h>
#include <cute_file_system.h>

#include <stdlib.h>

#include "../../engine/game_state.h"
#include "../../engine/log.h"
#include "bindings.h"

#define TAG "scripting"

static mrb_sym sym_update;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Load a file via the CF virtual filesystem, returning a heap-allocated
// null-terminated string. Caller must free().
static char* load_source(const char* path) {
  size_t size = 0;
  char* src = cf_fs_read_entire_file_to_memory_and_nul_terminate(path, &size);
  if (!src) {
    log_error(TAG, "Failed to read script: %s", path);
  }
  return src;
}

// Intern all scripting symbols that must survive across hot reloads.
// Called at init and after every hot reload (statics are re-zeroed on reload).
static void intern_symbols(mrb_state* mrb) {
  sym_update = mrb_intern_cstr(mrb, "update");
}

// Log and clear a pending mruby exception. Returns true if there was one.
static bool check_exc(mrb_state* mrb, const char* context) {
  if (!mrb->exc) {
    return false;
  }
  mrb_value exc = mrb_obj_value(mrb->exc);
  mrb_value msg = mrb_funcall(mrb, exc, "inspect", 0);
  log_error(TAG, "%s: %s", context, mrb_string_cstr(mrb, msg));
  mrb->exc = nullptr;
  return true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool scripting_init(void) {
  state->mrb = mrb_open();
  if (!state->mrb) {
    log_error(TAG, "Failed to create mruby VM");
    return false;
  }
  scripting_register_bindings(state->mrb);
  intern_symbols(state->mrb);
  log_info(TAG, "mruby VM initialized");

  scripting_load_file("assets/scripts/runtime.rb");
  scripting_load_file("assets/scripts/main.rb");
  return true;
}

void scripting_shutdown(void) {
  if (state->mrb) {
    mrb_close(state->mrb);
    state->mrb = nullptr;
    log_info(TAG, "mruby VM shut down");
  }
}

void scripting_hot_reload(void) {
  if (state->mrb) {
    scripting_register_bindings(state->mrb);
    intern_symbols(state->mrb);
    log_info(TAG, "mruby bindings re-registered after hot reload");
  }
}

// ---------------------------------------------------------------------------
// Per-frame update — calls Ruby update(dt) if defined at the top level
// ---------------------------------------------------------------------------

void scripting_update(float dt) {
  if (!state->mrb) {
    return;
  }
  mrb_state* mrb  = state->mrb;
  mrb_value  self = mrb_top_self(mrb);
  if (!mrb_respond_to(mrb, self, sym_update)) {
    return;
  }
  mrb_funcall(mrb, self, "update", 1, mrb_float_value(mrb, (mrb_float)dt));
  check_exc(mrb, "update(dt)");
}

// ---------------------------------------------------------------------------
// File loading — execute a script, discard return value
// ---------------------------------------------------------------------------

bool scripting_load_file(const char* path) {
  if (!state->mrb) {
    return false;
  }
  char* src = load_source(path);
  if (!src) {
    return false;
  }
  mrb_load_string(state->mrb, src);
  free(src);
  if (check_exc(state->mrb, path)) {
    return false;
  }
  log_info(TAG, "Loaded script: %s", path);
  return true;
}

// ---------------------------------------------------------------------------
// Fiber attachment — the script's last expression must evaluate to a Fiber
// ---------------------------------------------------------------------------

bool scripting_attach(ecs_entity_t entity, const char* path) {
  if (!state->mrb) {
    return false;
  }
  mrb_state* mrb = state->mrb;

  char* src = load_source(path);
  if (!src) {
    return false;
  }
  mrb_value fiber = mrb_load_string(mrb, src);
  free(src);
  if (check_exc(mrb, path)) {
    return false;
  }

  // Validate that the script returned a Fiber
  struct RClass* fiber_class = mrb_class_get(mrb, "Fiber");
  if (!mrb_obj_is_kind_of(mrb, fiber, fiber_class)) {
    log_error(TAG, "%s must return a Fiber (got %s)", path,
              mrb_obj_classname(mrb, fiber));
    return false;
  }

  // Register as a GC root so it survives between frames while held in C memory
  mrb_gc_register(mrb, fiber);

  // Create a cached Entity instance for this entity
  struct RClass* entity_cls = mrb_class_get(mrb, "Entity");
  mrb_value      id_val     = mrb_int_value(mrb, (mrb_int)entity.id);
  mrb_value      entity_obj = mrb_obj_new(mrb, entity_cls, 1, &id_val);
  mrb_gc_register(mrb, entity_obj);

  auto script            = ECS_ADD(entity, C_Script);
  script->fiber          = fiber;
  script->entity_obj     = entity_obj;
  script->path           = cf_sintern(path);
  script->active         = true;

  log_info(TAG, "Attached %s to entity %llu", path, entity.id);
  return true;
}

// ---------------------------------------------------------------------------
// Fiber resumption (used by sys_scripting)
// ---------------------------------------------------------------------------

void scripting_resume_fiber(C_Script* script) {
  mrb_state* mrb = state->mrb;
  mrb_fiber_resume(mrb, script->fiber, 1, &script->entity_obj);
  if (!mrb->exc) {
    return;
  }
  struct RClass* fiber_error_cls = mrb_class_get(mrb, "FiberError");
  if (mrb_obj_is_kind_of(mrb, mrb_obj_value(mrb->exc), fiber_error_cls)) {
    log_info(TAG, "fiber finished");
    mrb->exc = nullptr;
  } else {
    check_exc(mrb, "fiber resume");
  }
  script->active = false;
}
