cmake_minimum_required(VERSION 3.25)
project(tacticaltwo C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

# Hot-reload option: ON for development, OFF for release
option(ENABLE_HOT_RELOADING "Enable hot code reloading (shared library)" ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(vendor)
add_subdirectory(src)

# Common compile settings for all project targets
set(TARGETS game TacticalTwo)
foreach(target ${TARGETS})
    target_compile_features(${target} PRIVATE c_std_23)

    target_compile_options(${target} PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4;/D_CRT_SECURE_NO_WARNINGS;/Zc:preprocessor>
        $<$<AND:$<C_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/RTCcsu;/Zi>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wdouble-promotion;-Wstrict-prototypes;-Wmissing-prototypes;-fPIC>
        $<$<AND:$<C_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-fanalyzer>
    )

    target_compile_definitions(${target} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG>
        $<$<CONFIG:Release>:RELEASE>
        $<$<PLATFORM_ID:Windows>:PLATFORM_WINDOWS>
        $<$<PLATFORM_ID:Linux>:PLATFORM_LINUX>
        $<$<PLATFORM_ID:Darwin>:PLATFORM_MACOS>
        LOG_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src/"
    )

    target_include_directories(${target} SYSTEM PRIVATE
        ${PROJECT_SOURCE_DIR}/vendor/empyreanx
    )
endforeach()

# Output status of hot-reloading
message(STATUS "${PROJECT_NAME} => Hot-reloading is ${ENABLE_HOT_RELOADING}")
