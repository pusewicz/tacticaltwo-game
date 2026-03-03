###
# Dependencies
###

###
# Settings
###

if(ENABLE_HOT_RELOADING)
  # Use shared CF for hot-reload (both host and game link against same lib)
  set(CF_FRAMEWORK_STATIC OFF CACHE BOOL "" FORCE)
else()
  # Use static CF for release (single executable)
  set(CF_FRAMEWORK_STATIC ON CACHE BOOL "" FORCE)
endif()

###
# Add cute_framework
###

add_subdirectory(cute_framework EXCLUDE_FROM_ALL SYSTEM)

###
# Add mruby
###

include(ExternalProject)

set(MRUBY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mruby)
set(MRUBY_BUILD_DIR ${CMAKE_BINARY_DIR}/mruby-build)
set(MRUBY_LIB_FILE ${CMAKE_STATIC_LIBRARY_PREFIX}mruby${CMAKE_STATIC_LIBRARY_SUFFIX})

ExternalProject_Add(mruby_build
  SOURCE_DIR        ${MRUBY_SOURCE_DIR}
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ${CMAKE_COMMAND} -E env
                      MRUBY_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/mruby_build_config.rb
                      MRUBY_BUILD_DIR=${MRUBY_BUILD_DIR}
                      rake
  INSTALL_COMMAND   ""
  BUILD_BYPRODUCTS  ${MRUBY_BUILD_DIR}/host/lib/${MRUBY_LIB_FILE}
  BUILD_IN_SOURCE   TRUE
  BUILD_ALWAYS      FALSE
)

# Pre-create the generated include dir so CMake's INTERFACE_INCLUDE_DIRECTORIES
# check passes before mruby has been built for the first time.
file(MAKE_DIRECTORY ${MRUBY_BUILD_DIR}/host/include)

add_library(mruby STATIC IMPORTED GLOBAL)
set_target_properties(mruby PROPERTIES
  IMPORTED_LOCATION ${MRUBY_BUILD_DIR}/host/lib/${MRUBY_LIB_FILE}
)
target_include_directories(mruby SYSTEM INTERFACE
  ${MRUBY_SOURCE_DIR}/include
  ${MRUBY_BUILD_DIR}/host/include
)
if(UNIX)
  target_link_libraries(mruby INTERFACE m)
endif()
add_dependencies(mruby mruby_build)
