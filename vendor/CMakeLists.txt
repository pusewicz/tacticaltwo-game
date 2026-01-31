include(FetchContent)

###
# Dependencies
###

FetchContent_Declare(
  cute
  GIT_REPOSITORY https://github.com/RandyGaul/cute_framework.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  FETCHCONTENT_FULLY_DISCONNECTED
)

###
# Settings
###

set(CF_FRAMEWORK_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(CF_FRAMEWORK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(ENABLE_HOT_RELOADING)
  # Use shared CF for hot-reload (both host and game link against same lib)
  set(CF_FRAMEWORK_STATIC OFF CACHE BOOL "" FORCE)
else()
  # Use static CF for release (single executable)
  set(CF_FRAMEWORK_STATIC ON CACHE BOOL "" FORCE)
endif()

###
# Populate
###

FetchContent_MakeAvailable(cute)

# Mark cute headers as system to suppress warnings
get_target_property(CUTE_INCLUDE_DIRS cute INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(cute PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${CUTE_INCLUDE_DIRS}")
