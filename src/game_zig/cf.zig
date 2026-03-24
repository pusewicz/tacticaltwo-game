// cf.zig — Thin Zig wrapper over Cute Framework's C API.
//
// Uses @cImport to pull in CF headers directly. This file re-exports the raw C
// namespace and provides Zig-idiomatic helpers where the C API is awkward.

pub const c = @cImport({
    @cInclude("cute_alloc.h");
    @cInclude("cute_app.h");
    @cInclude("cute_color.h");
    @cInclude("cute_defines.h");
    @cInclude("cute_draw.h");
    @cInclude("cute_graphics.h");
    @cInclude("cute_input.h");
    @cInclude("cute_math.h");
    @cInclude("cute_sprite.h");
    @cInclude("cute_time.h");
    @cInclude("cute_file_system.h");
    @cInclude("cute_coroutine.h");
});

// Re-export commonly used types at the top level for convenience.
pub const Canvas = c.CF_Canvas;
pub const Arena = c.CF_Arena;
pub const V2 = c.CF_V2;
pub const Sprite = c.CF_Sprite;
pub const Color = c.CF_Color;
pub const RenderState = c.CF_RenderState;
pub const CanvasParams = c.CF_CanvasParams;

// --- Idiomatic Zig helpers ---

pub fn v2(x: f32, y: f32) V2 {
    return .{ .x = x, .y = y };
}

pub fn ortho2d(x: f32, y: f32, w: f32, h: f32) c.CF_M3x2 {
    return c.cf_ortho_2d(x, y, w, h);
}

pub fn deltaTime() f32 {
    return c.CF_DELTA_TIME;
}

pub fn keyDown(key: c_int) bool {
    return c.cf_key_down(@intCast(key));
}

pub fn keyJustPressed(key: c_int) bool {
    return c.cf_key_just_pressed(@intCast(key));
}

pub fn clearColor(r: f32, g: f32, b: f32, a: f32) void {
    c.cf_clear_color(r, g, b, a);
}

pub fn clearCanvas(canvas: Canvas) void {
    c.cf_clear_canvas(canvas);
}

pub fn renderTo(canvas: Canvas, clear: bool) void {
    c.cf_render_to(canvas, clear);
}

pub fn drawProjection(m: c.CF_M3x2) void {
    c.cf_draw_projection(m);
}

pub fn drawCanvas(canvas: Canvas, pos: V2, size: V2) void {
    c.cf_draw_canvas(canvas, pos, size);
}

pub fn drawPushFilter(filter: c_int) void {
    c.cf_draw_push_filter(@intCast(filter));
}

pub fn drawPopFilter() void {
    c.cf_draw_pop_filter();
}

pub fn drawPushRenderState(rs: RenderState) void {
    c.cf_draw_push_render_state(rs);
}

pub fn drawPopRenderState() void {
    c.cf_draw_pop_render_state();
}

pub fn makeCanvas(params: CanvasParams) Canvas {
    return c.cf_make_canvas(params);
}

pub fn destroyCanvas(canvas: Canvas) void {
    c.cf_destroy_canvas(canvas);
}

pub fn canvasDefaults(w: c_int, h: c_int) CanvasParams {
    return c.cf_canvas_defaults(w, h);
}

pub fn appGetWidth() c_int {
    return c.cf_app_get_width();
}

pub fn appGetHeight() c_int {
    return c.cf_app_get_height();
}

pub fn appSetCanvasSize(w: c_int, h: c_int) void {
    c.cf_app_set_canvas_size(w, h);
}

pub fn renderStateDefaults() RenderState {
    return c.cf_render_state_defaults();
}

pub fn makeArena(alignment: c_int, block_size: c_int) Arena {
    return c.cf_make_arena(alignment, block_size);
}

pub fn arenaReset(arena: *Arena) void {
    c.cf_arena_reset(arena);
}

pub fn destroyArena(arena: *Arena) void {
    c.cf_destroy_arena(arena);
}

pub fn shaderDirectory(path: [*:0]const u8) void {
    c.cf_shader_directory(path);
}

pub fn appInitImgui() void {
    c.cf_app_init_imgui();
}
