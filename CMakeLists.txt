cmake_minimum_required(VERSION 3.20)
project(parallel_pawns LANGUAGES CXX)

# Options
option(ENABLE_NATIVE "Enable -march=native" ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Export compile_commands.json (clangd/clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compile settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(engine_warnings INTERFACE)
target_compile_options(engine_warnings INTERFACE
  -Wall -Wextra -Wpedantic
)

if(ENABLE_NATIVE)
  target_compile_options(engine_warnings INTERFACE -march=native)
endif()

# Debug => -g -O0 -DDEBUG
# Release => -O3 -DNDEBUG and LTO
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUG)
  add_library(engine_opts INTERFACE)
  target_compile_options(engine_opts INTERFACE -O0 -g)
else()
  add_library(engine_opts INTERFACE)
  target_compile_options(engine_opts INTERFACE -O3)
  add_compile_definitions(NDEBUG)

  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_err)
  if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)  # enables LTO where supported
  endif()
endif()

# Engine executable
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/engine/*.cpp"
)

add_executable(engine ${ENGINE_SOURCES})
target_include_directories(engine PRIVATE
  "${CMAKE_SOURCE_DIR}/src/engine"
)
target_link_libraries(engine PRIVATE engine_warnings engine_opts)

set_target_properties(engine PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
  OUTPUT_NAME "engine"
)

# Tools
add_executable(find_magics tools/find_magics.cpp)
target_include_directories(find_magics PRIVATE
  "${CMAKE_SOURCE_DIR}/src/engine"
)
target_link_libraries(find_magics PRIVATE engine_warnings engine_opts)
set_target_properties(find_magics PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)

# Convenience targets
add_custom_target(run
  COMMAND "${CMAKE_SOURCE_DIR}/build/engine"
  DEPENDS engine
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# clang-format targets
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
  file(GLOB_RECURSE ALL_CPP_H CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp" "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/tools/*.cpp" "${CMAKE_SOURCE_DIR}/tools/*.h"
  )

  add_custom_target(format
    COMMAND "${CLANG_FORMAT_EXE}" -i ${ALL_CPP_H}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )

  add_custom_target(check-format
    COMMAND "${CLANG_FORMAT_EXE}" --dry-run --Werror ${ALL_CPP_H}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  )
endif()