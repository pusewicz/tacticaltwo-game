set(NAME "TacticalTwo")

add_executable(${NAME} WIN32 MACOSX_BUNDLE
  main.c
  ../engine/log.c
  ../platform/platform_cute.c
)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${NAME} PRIVATE config cute)

if(ENABLE_HOT_RELOADING)
  target_compile_definitions(${NAME} PRIVATE ENGINE_HOT_RELOADING)
  target_compile_definitions(${NAME} PRIVATE
    GAME_LIB_BASENAME="game"
    GAME_LIB_PREFIX="${CMAKE_SHARED_LIBRARY_PREFIX}"
    GAME_LIB_SUFFIX="${CMAKE_SHARED_LIBRARY_SUFFIX}"
    ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets" # Pass source assets path for development (CF fs doesn't follow symlinks)
  )
else()
  message(FATAL_ERROR "Release build with hot-reloading disabled is not yet supported (should link to game or better embed directly with source files).")
endif()
