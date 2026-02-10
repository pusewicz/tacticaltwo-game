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
