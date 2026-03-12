set(NAME "game")

add_library(${NAME} SHARED
  game.c
  world.c
  ldtk.c
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
