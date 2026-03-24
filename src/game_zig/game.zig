// game.zig — Zig implementation of the game shared library.
//
// This is the Phase 1 proof-of-concept: a minimal game that exports the same
// 6 C-ABI functions as the C version and calls Cute Framework directly.
// It renders a colored background and a moving box to prove the pipeline works.

const std = @import("std");
const cf = @import("cf.zig");

// Import our C config constants.
const config = @cImport({
    @cInclude("config.h");
});

const CANVAS_WIDTH: c_int = config.CANVAS_WIDTH;
const CANVAS_HEIGHT: c_int = config.CANVAS_HEIGHT;
const CANVAS_SCALE: f32 = @floatFromInt(config.CANVAS_SCALE);

// --- Game State ---
// For Phase 1 we use a simple Zig-owned state rather than the full C GameState.
// In Phase 2+, this will grow to hold World, entities, etc.

const GameState = struct {
    canvas: cf.Canvas = undefined,
    box_x: f32 = 0,
    box_y: f32 = 135,
    box_speed: f32 = 120,
    box_dir: f32 = 1,
    initialized: bool = false,
};

var state: *GameState = undefined;

// --- Helpers ---

fn calculateDestSize(game: cf.V2, window: cf.V2) cf.V2 {
    const game_aspect = game.x / game.y;
    const window_aspect = window.x / window.y;

    if (window_aspect > game_aspect) {
        // Pillarbox
        const dest_h = window.y;
        const dest_w = dest_h * game_aspect;
        return cf.v2(dest_w, dest_h);
    } else {
        // Letterbox
        const dest_w = window.x;
        const dest_h = dest_w / game_aspect;
        return cf.v2(dest_w, dest_h);
    }
}

// --- Exported C-ABI functions ---

const Platform = opaque {};

export fn game_init(_: ?*Platform) callconv(.c) void {
    const alloc = std.heap.c_allocator;
    state = alloc.create(GameState) catch @panic("failed to allocate GameState");
    state.* = .{};

    state.canvas = cf.makeCanvas(cf.canvasDefaults(CANVAS_WIDTH, CANVAS_HEIGHT));

    cf.drawProjection(cf.ortho2d(
        0,
        0,
        @as(f32, @floatFromInt(CANVAS_WIDTH)) * CANVAS_SCALE,
        @as(f32, @floatFromInt(CANVAS_HEIGHT)) * CANVAS_SCALE,
    ));

    cf.shaderDirectory("/assets/shaders");
    cf.appInitImgui();

    state.initialized = true;
}

export fn game_update() callconv(.c) bool {
    if (!state.initialized) return true;

    const dt = cf.deltaTime();

    // Simple box animation: bounce left-right
    state.box_x += state.box_speed * state.box_dir * dt;
    if (state.box_x > @as(f32, @floatFromInt(CANVAS_WIDTH)) - 32) {
        state.box_dir = -1;
    }
    if (state.box_x < 0) {
        state.box_dir = 1;
    }

    // Toggle debug with G key
    if (cf.keyJustPressed(cf.c.CF_KEY_G)) {
        // placeholder for debug toggle
    }

    return true;
}

export fn game_render() callconv(.c) void {
    if (!state.initialized) return;

    cf.drawPushFilter(cf.c.CF_DRAW_FILTER_NEAREST);

    // PASS 1: Render to game canvas
    {
        // Cornflower blue background
        cf.clearColor(100.0 / 255.0, 149.0 / 255.0, 237.0 / 255.0, 1.0);
        cf.clearCanvas(state.canvas);

        // Draw a white box to prove rendering works
        cf.c.cf_draw_push_color(cf.c.cf_make_color_rgba_f(1, 1, 1, 1));
        cf.c.cf_draw_box_fill(cf.c.cf_make_aabb_center_half_extents(
            cf.v2(state.box_x + 16, state.box_y),
            cf.v2(16, 16),
        ));
        cf.c.cf_draw_pop_color();

        cf.renderTo(state.canvas, true);
    }

    // PASS 2: Draw canvas to screen with letterboxing
    {
        const window_w = cf.appGetWidth();
        const window_h = cf.appGetHeight();
        cf.appSetCanvasSize(window_w, window_h);
        cf.clearColor(0, 0, 0, 1.0);

        const dest = calculateDestSize(
            cf.v2(@floatFromInt(CANVAS_WIDTH), @floatFromInt(CANVAS_HEIGHT)),
            cf.v2(@floatFromInt(window_w), @floatFromInt(window_h)),
        );
        cf.drawProjection(cf.ortho2d(0, 0, @floatFromInt(window_w), @floatFromInt(window_h)));
        cf.drawCanvas(state.canvas, cf.v2(0, 0), dest);

        // Restore game projection
        cf.drawProjection(cf.ortho2d(
            0,
            0,
            @as(f32, @floatFromInt(CANVAS_WIDTH)) * CANVAS_SCALE,
            @as(f32, @floatFromInt(CANVAS_HEIGHT)) * CANVAS_SCALE,
        ));
    }

    cf.drawPopFilter();
}

export fn game_shutdown() callconv(.c) void {
    if (!state.initialized) return;

    cf.destroyCanvas(state.canvas);

    const alloc = std.heap.c_allocator;
    alloc.destroy(state);
}

export fn game_state() callconv(.c) ?*anyopaque {
    return @ptrCast(state);
}

export fn game_hot_reload(game_state_ptr: ?*anyopaque) callconv(.c) void {
    if (game_state_ptr) |ptr| {
        state = @ptrCast(@alignCast(ptr));
    }
}
