set(NAME "game")

add_library(${NAME} SHARED
  game.c
  world.c
  ../engine/asset.c
  ../engine/log.c
)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${NAME} PRIVATE cute)

if(ENABLE_HOT_RELOADING)
  # Copy game library to appropriate location
  if(APPLE)
    # macOS: SDL_GetBasePath() returns Contents/Resources/ for app bundles
    add_custom_command(TARGET ${NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TacticalTwo.app/Contents/Resources
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:game>
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/TacticalTwo.app/Contents/Resources/
    )
  else()
    # Windows/Linux: copy next to executable
    add_custom_command(TARGET ${NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:game>
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    )
  endif()
else()
  message(FATAL_ERROR "Release build with hot-reloading disabled is not yet supported.")
endif()
