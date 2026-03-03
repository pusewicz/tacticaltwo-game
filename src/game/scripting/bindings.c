// bindings.c - C-to-Ruby bindings for game APIs

#include "bindings.h"

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/value.h>
#include <cute_math.h>

#include "../../engine/game_state.h"
#include "../../engine/log.h"
#include "../world.h"

// ---------------------------------------------------------------------------
// Shared symbols
// ---------------------------------------------------------------------------

static mrb_sym sym_id;        // @id on Entity
static mrb_sym sym_comp_id;   // @comp_id on component classes (stores ecs_comp_t id)
static mrb_sym sym_entity_id; // @entity_id on proxy instances

// ---------------------------------------------------------------------------
// Shared proxy helpers
// ---------------------------------------------------------------------------

// Extract an entity from an ivar holding a raw integer entity ID.
static inline ecs_entity_t entity_from_ivar(mrb_state* mrb, mrb_value self, mrb_sym sym) {
  mrb_value id_val = mrb_iv_get(mrb, self, sym);
  return (ecs_entity_t){ .id = (ecs_id_t)mrb_integer(id_val) };
}

// Single initialize used by all proxy classes — stores entity ID as @entity_id.
static mrb_value proxy_initialize(mrb_state* mrb, mrb_value self) {
  mrb_int id;
  mrb_get_args(mrb, "i", &id);
  mrb_iv_set(mrb, self, sym_entity_id, mrb_int_value(mrb, id));
  return self;
}

// ---------------------------------------------------------------------------
// Getter / setter code-gen macros for proxy component fields
// ---------------------------------------------------------------------------

#define DEFINE_PROXY_GETTER(NAME, FROM_SELF, FIELD)                            \
  static mrb_value NAME(mrb_state* mrb, mrb_value self) {                      \
    auto comp = FROM_SELF(mrb, self);                                           \
    if (!comp) { return mrb_nil_value(); }                                      \
    return mrb_float_value(mrb, (mrb_float)(comp->FIELD));                     \
  }

#define DEFINE_PROXY_SETTER(NAME, FROM_SELF, FIELD)                            \
  static mrb_value NAME(mrb_state* mrb, mrb_value self) {                      \
    mrb_float v;                                                                \
    mrb_get_args(mrb, "f", &v);                                                 \
    auto comp = FROM_SELF(mrb, self);                                           \
    if (!comp) { return mrb_nil_value(); }                                      \
    comp->FIELD = (float)v;                                                     \
    return mrb_float_value(mrb, v);                                             \
  }

// ---------------------------------------------------------------------------
// Game module
// ---------------------------------------------------------------------------

static mrb_value game_dt(mrb_state* mrb, [[maybe_unused]] mrb_value self) {
  return mrb_float_value(mrb, (mrb_float)state->world.dt);
}

// ---------------------------------------------------------------------------
// Entity class
//
// Instance methods. The entity ID is stored as @id in each instance.
// Instances are created once in scripting_attach and cached in C_Script.
// ---------------------------------------------------------------------------

static ecs_entity_t entity_from_self(mrb_state* mrb, mrb_value self) {
  return entity_from_ivar(mrb, self, sym_id);
}

static mrb_value entity_initialize(mrb_state* mrb, mrb_value self) {
  mrb_int id;
  mrb_get_args(mrb, "i", &id);
  mrb_iv_set(mrb, self, sym_id, mrb_int_value(mrb, id));
  return self;
}

static mrb_value entity_id(mrb_state* mrb, mrb_value self) {
  return mrb_iv_get(mrb, self, sym_id);
}

static mrb_value entity_inspect(mrb_state* mrb, mrb_value self) {
  mrb_value id_val = mrb_iv_get(mrb, self, sym_id);
  return mrb_format(mrb, "#<Entity:%v>", id_val);
}

static mrb_value entity_position(mrb_state* mrb, mrb_value self) {
  ecs_entity_t e = entity_from_self(mrb, self);
  if (!ECS_HAS(e, C_Transform)) {
    return mrb_nil_value();
  }
  auto xform = ECS_GET(e, C_Transform);
  mrb_value pos = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, pos, mrb_float_value(mrb, (mrb_float)xform->position.x));
  mrb_ary_push(mrb, pos, mrb_float_value(mrb, (mrb_float)xform->position.y));
  return pos;
}

static mrb_value entity_set_position(mrb_state* mrb, mrb_value self) {
  mrb_float x;
  mrb_float y;
  mrb_get_args(mrb, "ff", &x, &y);
  ecs_entity_t e = entity_from_self(mrb, self);
  if (!ECS_HAS(e, C_Transform)) {
    return mrb_nil_value();
  }
  auto xform = ECS_GET(e, C_Transform);
  xform->position = cf_v2((float)x, (float)y);
  return mrb_nil_value();
}

static mrb_value entity_set_velocity(mrb_state* mrb, mrb_value self) {
  mrb_float vx;
  mrb_float vy;
  mrb_get_args(mrb, "ff", &vx, &vy);
  ecs_entity_t e = entity_from_self(mrb, self);
  if (!ECS_HAS(e, C_Velocity)) {
    return mrb_nil_value();
  }
  auto vel = ECS_GET(e, C_Velocity);
  *vel = cf_v2((float)vx, (float)vy);
  return mrb_nil_value();
}

// ---------------------------------------------------------------------------
// Velocity proxy class
// ---------------------------------------------------------------------------

static struct RClass* velocity_cls;

static C_Velocity* velocity_from_self(mrb_state* mrb, mrb_value self) {
  ecs_entity_t e = entity_from_ivar(mrb, self, sym_entity_id);
  if (!ECS_HAS(e, C_Velocity)) {
    return nullptr;
  }
  return ECS_GET(e, C_Velocity);
}

DEFINE_PROXY_GETTER(velocity_x,     velocity_from_self, x)
DEFINE_PROXY_SETTER(velocity_set_x, velocity_from_self, x)
DEFINE_PROXY_GETTER(velocity_y,     velocity_from_self, y)
DEFINE_PROXY_SETTER(velocity_set_y, velocity_from_self, y)

static mrb_value velocity_to_a(mrb_state* mrb, mrb_value self) {
  auto vel = velocity_from_self(mrb, self);
  if (!vel) {
    return mrb_ary_new(mrb);
  }
  mrb_value v = mrb_ary_new_capa(mrb, 2);
  mrb_ary_push(mrb, v, mrb_float_value(mrb, (mrb_float)vel->x));
  mrb_ary_push(mrb, v, mrb_float_value(mrb, (mrb_float)vel->y));
  return v;
}

static mrb_value velocity_inspect(mrb_state* mrb, mrb_value self) {
  auto vel = velocity_from_self(mrb, self);
  if (!vel) {
    return mrb_str_new_cstr(mrb, "#<Velocity (missing)>");
  }
  return mrb_format(mrb, "#<Velocity x=%f y=%f>", (double)vel->x, (double)vel->y);
}

// ---------------------------------------------------------------------------
// Transform proxy class
// ---------------------------------------------------------------------------

static struct RClass* transform_cls;

static C_Transform* transform_from_self(mrb_state* mrb, mrb_value self) {
  ecs_entity_t e = entity_from_ivar(mrb, self, sym_entity_id);
  if (!ECS_HAS(e, C_Transform)) {
    return nullptr;
  }
  return ECS_GET(e, C_Transform);
}

DEFINE_PROXY_GETTER(transform_x,            transform_from_self, position.x)
DEFINE_PROXY_SETTER(transform_set_x,        transform_from_self, position.x)
DEFINE_PROXY_GETTER(transform_y,            transform_from_self, position.y)
DEFINE_PROXY_SETTER(transform_set_y,        transform_from_self, position.y)
DEFINE_PROXY_GETTER(transform_rotation,     transform_from_self, rotation)
DEFINE_PROXY_SETTER(transform_set_rotation, transform_from_self, rotation)

static mrb_value transform_inspect(mrb_state* mrb, mrb_value self) {
  auto xform = transform_from_self(mrb, self);
  if (!xform) {
    return mrb_str_new_cstr(mrb, "#<Transform (missing)>");
  }
  return mrb_format(mrb, "#<Transform x=%f y=%f rotation=%f>",
                    (double)xform->position.x,
                    (double)xform->position.y,
                    (double)xform->rotation);
}

// ---------------------------------------------------------------------------
// Entity proxy accessors and component query methods
// ---------------------------------------------------------------------------

static mrb_value entity_velocity(mrb_state* mrb, mrb_value self) {
  ecs_entity_t e = entity_from_self(mrb, self);
  if (!ECS_HAS(e, C_Velocity)) {
    return mrb_nil_value();
  }
  mrb_value id_val = mrb_iv_get(mrb, self, sym_id);
  return mrb_obj_new(mrb, velocity_cls, 1, &id_val);
}

static mrb_value entity_transform(mrb_state* mrb, mrb_value self) {
  ecs_entity_t e = entity_from_self(mrb, self);
  if (!ECS_HAS(e, C_Transform)) {
    return mrb_nil_value();
  }
  mrb_value id_val = mrb_iv_get(mrb, self, sym_id);
  return mrb_obj_new(mrb, transform_cls, 1, &id_val);
}

static mrb_value entity_has(mrb_state* mrb, mrb_value self) {
  mrb_value comp_class;
  mrb_get_args(mrb, "C", &comp_class);
  mrb_value comp_id_val = mrb_iv_get(mrb, comp_class, sym_comp_id);
  if (mrb_nil_p(comp_id_val)) {
    return mrb_false_value();
  }
  ecs_comp_t   comp = { .id = (ecs_id_t)mrb_integer(comp_id_val) };
  ecs_entity_t e    = entity_from_self(mrb, self);
  return mrb_bool_value(ecs_has(state->world.ecs, e, comp));
}

static mrb_value entity_get(mrb_state* mrb, mrb_value self) {
  mrb_value comp_class;
  mrb_get_args(mrb, "C", &comp_class);
  mrb_value comp_id_val = mrb_iv_get(mrb, comp_class, sym_comp_id);
  if (mrb_nil_p(comp_id_val)) {
    return mrb_nil_value();
  }
  ecs_comp_t   comp = { .id = (ecs_id_t)mrb_integer(comp_id_val) };
  ecs_entity_t e    = entity_from_self(mrb, self);
  if (!ecs_has(state->world.ecs, e, comp)) {
    return mrb_nil_value();
  }
  mrb_value      id_val = mrb_iv_get(mrb, self, sym_id);
  struct RClass* cls    = mrb_class_ptr(comp_class);
  return mrb_obj_new(mrb, cls, 1, &id_val);
}

// ---------------------------------------------------------------------------

void scripting_register_bindings(mrb_state* mrb) {
  sym_id        = mrb_intern_lit(mrb, "@id");
  sym_comp_id   = mrb_intern_lit(mrb, "@comp_id");
  sym_entity_id = mrb_intern_lit(mrb, "@entity_id");

  struct RClass* game_mod = mrb_define_module(mrb, "Game");
  mrb_define_class_method(mrb, game_mod, "dt", game_dt, MRB_ARGS_NONE());

  // Velocity proxy class
  velocity_cls = mrb_define_class(mrb, "Velocity", mrb->object_class);
  mrb_iv_set(mrb, mrb_obj_value(velocity_cls), sym_comp_id,
             mrb_int_value(mrb, (mrb_int)ECS_GET_COMP(C_Velocity).id));
  mrb_define_method(mrb, velocity_cls, "initialize", proxy_initialize,    MRB_ARGS_REQ(1));
  mrb_define_method(mrb, velocity_cls, "x",          velocity_x,          MRB_ARGS_NONE());
  mrb_define_method(mrb, velocity_cls, "x=",         velocity_set_x,      MRB_ARGS_REQ(1));
  mrb_define_method(mrb, velocity_cls, "y",          velocity_y,          MRB_ARGS_NONE());
  mrb_define_method(mrb, velocity_cls, "y=",         velocity_set_y,      MRB_ARGS_REQ(1));
  mrb_define_method(mrb, velocity_cls, "to_a",       velocity_to_a,       MRB_ARGS_NONE());
  mrb_define_method(mrb, velocity_cls, "inspect",    velocity_inspect,    MRB_ARGS_NONE());

  // Transform proxy class
  transform_cls = mrb_define_class(mrb, "Transform", mrb->object_class);
  mrb_iv_set(mrb, mrb_obj_value(transform_cls), sym_comp_id,
             mrb_int_value(mrb, (mrb_int)ECS_GET_COMP(C_Transform).id));
  mrb_define_method(mrb, transform_cls, "initialize", proxy_initialize,       MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform_cls, "x",          transform_x,            MRB_ARGS_NONE());
  mrb_define_method(mrb, transform_cls, "x=",         transform_set_x,        MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform_cls, "y",          transform_y,            MRB_ARGS_NONE());
  mrb_define_method(mrb, transform_cls, "y=",         transform_set_y,        MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform_cls, "rotation",   transform_rotation,     MRB_ARGS_NONE());
  mrb_define_method(mrb, transform_cls, "rotation=",  transform_set_rotation, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, transform_cls, "inspect",    transform_inspect,      MRB_ARGS_NONE());

  // Entity class
  struct RClass* entity_cls = mrb_define_class(mrb, "Entity", mrb->object_class);
  mrb_define_method(mrb, entity_cls, "initialize",   entity_initialize,   MRB_ARGS_REQ(1));
  mrb_define_method(mrb, entity_cls, "id",           entity_id,           MRB_ARGS_NONE());
  mrb_define_method(mrb, entity_cls, "inspect",      entity_inspect,      MRB_ARGS_NONE());
  mrb_define_method(mrb, entity_cls, "position",     entity_position,     MRB_ARGS_NONE());
  mrb_define_method(mrb, entity_cls, "set_position", entity_set_position, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, entity_cls, "velocity",     entity_velocity,     MRB_ARGS_NONE());
  mrb_define_method(mrb, entity_cls, "set_velocity", entity_set_velocity, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, entity_cls, "transform",    entity_transform,    MRB_ARGS_NONE());
  mrb_define_method(mrb, entity_cls, "has?",         entity_has,          MRB_ARGS_REQ(1));
  mrb_define_method(mrb, entity_cls, "get",          entity_get,          MRB_ARGS_REQ(1));

  log_debug("bindings", "Game module, Velocity/Transform proxies, and Entity class bindings registered");
}
