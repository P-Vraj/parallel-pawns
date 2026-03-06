cmake_minimum_required(VERSION 3.20)

# CCache support for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

project(parallel_pawns LANGUAGES CXX)

# Options
option(ENABLE_NATIVE "Enable -march=native" ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Export compile_commands.json for clang tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compile settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use Catch2 for testing
include(FetchContent)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.3
)

FetchContent_MakeAvailable(catch2)

# Tools we may reference in custom targets
find_program(CLANG_FORMAT_EXE NAMES clang-format)

add_library(engine_warnings INTERFACE)
target_compile_options(engine_warnings INTERFACE
  -Wall -Wextra -Wpedantic
)

if(ENABLE_NATIVE)
  target_compile_options(engine_warnings INTERFACE -march=native)
endif()

# Debug: -g -O0 -DDEBUG
# Release: -O3 and LTO
add_library(engine_opts INTERFACE)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(engine_opts INTERFACE -O0 -g)
  add_compile_definitions(DEBUG)
else()
  target_compile_options(engine_opts INTERFACE -O3)

  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_err)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)  # enables LTO where supported
  endif()
endif()

# Engine core library target
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/engine/*.cpp"
)

add_library(engine_core ${ENGINE_SOURCES})
target_include_directories(engine_core PUBLIC
  "${CMAKE_SOURCE_DIR}/src/engine"
)
target_link_libraries(engine_core PUBLIC engine_warnings engine_opts)

# Engine executable (thin wrapper over engine_core)
# Removes engine.cpp from engine_core
get_target_property(_engine_core_sources engine_core SOURCES)
foreach(_src IN LISTS _engine_core_sources)
  if(_src MATCHES ".*/engine\\.cpp$")
    list(REMOVE_ITEM _engine_core_sources "${_src}")
    set_target_properties(engine_core PROPERTIES SOURCES "${_engine_core_sources}")
    set(_engine_main "${_src}")
  endif()
endforeach()

if(DEFINED _engine_main)
  add_executable(engine "${_engine_main}")
else()
  # Main source
  add_executable(engine "${CMAKE_SOURCE_DIR}/src/engine/engine.cpp")
endif()

target_link_libraries(engine PRIVATE engine_core)

# Tools
add_executable(find_magics tools/find_magics.cpp)
target_include_directories(find_magics PRIVATE
  "${CMAKE_SOURCE_DIR}/src/engine"
)
target_link_libraries(find_magics PRIVATE engine_core)

add_custom_target(run-find-magics
  COMMAND $<TARGET_FILE:find_magics> > ${CMAKE_SOURCE_DIR}/src/engine/move_gen/magics_generated.h
  DEPENDS find_magics
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  USES_TERMINAL
  VERBATIM
)

# Format generated file if clang-format exists
if(CLANG_FORMAT_EXE)
  add_custom_command(TARGET run-find-magics POST_BUILD
    COMMAND ${CLANG_FORMAT_EXE} -i ${CMAKE_SOURCE_DIR}/src/engine/move_gen/magics_generated.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
  )
endif()

# Gather all .h/.cpp files for clang-format/clang-tidy targets
file(GLOB_RECURSE ALL_CPP_H CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h"
  "${CMAKE_SOURCE_DIR}/tools/*.cpp" "${CMAKE_SOURCE_DIR}/tools/*.h"
  "${CMAKE_SOURCE_DIR}/tests/*.cpp" "${CMAKE_SOURCE_DIR}/tests/*.h"
)

# clang-tidy target
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
  add_custom_target(tidy
    COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --quiet
            --format-style=file
            ${ALL_CPP_H}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
  )
endif()

# Convenience targets
add_custom_target(run
  COMMAND $<TARGET_FILE:engine>
  DEPENDS engine
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# clang-format targets
if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXE}" -i ${ALL_CPP_H}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )

  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${ALL_CPP_H}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endif()

# Tests
add_executable(perft_tests
  tests/engine/perft.cpp
)

# Mark Catch2 as SYSTEM to suppress linter warnings
target_include_directories(perft_tests SYSTEM PRIVATE
  $<TARGET_PROPERTY:Catch2::Catch2WithMain,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(perft_tests
  PRIVATE
  engine_core
  Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(perft_tests)