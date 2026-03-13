set(NAME "game")

add_library(${NAME} SHARED
  game.c
  world.c
  ldtk.c
  lighting.c
  rain.c
  systems/input_system.c
  systems/player_system.c
  systems/physics_system.c
  systems/animation_system.c
  systems/render_system.c
  systems/tilemap_system.c
  systems/camera_system.c
  systems/parallax_system.c
  ../engine/asset.c
  ../engine/log.c
)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${NAME} PRIVATE cute)

if(NOT ENABLE_HOT_RELOADING)
  message(FATAL_ERROR "Release build with hot-reloading disabled is not yet supported.")
endif()

# macOS: linker-signed ad-hoc signatures are rejected by dyld when the library
# is copied for hot-reload cache-busting. Replace with an explicit ad-hoc sig.
if(APPLE)
  add_custom_command(TARGET ${NAME} POST_BUILD
    COMMAND codesign --force --sign - $<TARGET_FILE:${NAME}>
    COMMENT "Ad-hoc codesign ${NAME} for hot-reload dlopen")
endif()
