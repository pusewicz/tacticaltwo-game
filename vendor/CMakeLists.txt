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
