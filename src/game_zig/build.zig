const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // --- Game shared library (replaces the C libgame.so) ---
    const game_lib = b.addSharedLibrary(.{
        .name = "game",
        .root_source_file = b.path("game.zig"),
        .target = target,
        .optimize = optimize,
    });

    // Cute Framework headers
    const cf_root = "../../vendor/cute_framework";
    game_lib.addIncludePath(b.path(cf_root ++ "/include"));
    // dcimgui headers (used by CF's app layer)
    game_lib.addIncludePath(b.path(cf_root ++ "/libraries/imgui"));

    // Our own C headers (engine, config)
    game_lib.addIncludePath(b.path("../engine"));
    game_lib.addIncludePath(b.path("../config"));

    // Link against the CF shared library built by CMake.
    // The caller must pass -Dcf-lib-path=<path> pointing to the directory
    // containing libcute.so (typically build/relwithdebinfo/bin).
    const cf_lib_path = b.option(
        []const u8,
        "cf-lib-path",
        "Path to directory containing the built Cute Framework shared library",
    ) orelse "../../build/relwithdebinfo/bin";
    game_lib.addLibraryPath(.{ .cwd_relative = cf_lib_path });
    game_lib.linkSystemLibrary("cute");

    // Link libc (needed for CF's C runtime and stdlib calls)
    game_lib.linkLibC();

    // Install the shared library into zig-out/lib/
    b.installArtifact(game_lib);

    // --- Convenience: copy the .so next to the C executable ---
    const install_to_bin = b.addInstallArtifact(game_lib, .{
        .dest_dir = .{ .override = .{ .custom = "../../../build/relwithdebinfo/bin" } },
    });

    const deploy_step = b.step("deploy", "Build and copy libgame to the game's bin directory");
    deploy_step.dependOn(&install_to_bin.step);
}
