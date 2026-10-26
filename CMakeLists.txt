cmake_minimum_required(VERSION 3.20)
project(ProjectY VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(PROJECTY_BUILD_GAME  "Build the game executable (needs SDL2)" ON)
option(PROJECTY_BUILD_TESTS "Build the unit tests" ON)
option(PROJECTY_FETCH_SDL2  "Download SDL2 from GitHub if it is not installed" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Strict floating point (see docs/DETERMINISM.md)
#
# The original game did every float operation in software, IEEE binary32,
# round-to-nearest, in source order. Multiplayer is deterministic lockstep,
# so every build of the port must produce bit-identical results:
#   - no FMA contraction (a*b+c must round twice)
#   - no fast-math / reassociation / reciprocal approximations
#   - SSE2 scalar math on 32-bit x86 (never the x87 FPU)
# ---------------------------------------------------------------------------
function(projecty_strict_float target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /fp:precise /fp:contract-)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      target_compile_options(${target} PRIVATE /arch:SSE2)
    endif()
  else()
    target_compile_options(${target} PRIVATE -ffp-contract=off -fno-fast-math)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE -fexcess-precision=standard -frounding-math -fsignaling-nans)
    endif()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$" OR CMAKE_SIZEOF_VOID_P EQUAL 4)
      if(CMAKE_SYSTEM_PROCESSOR MATCHES "86")
        target_compile_options(${target} PRIVATE -msse2 -mfpmath=sse)
      endif()
    endif()
  endif()
endfunction()

function(projecty_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library: deterministic engine code (no platform dependencies)
# ---------------------------------------------------------------------------
add_library(projecty_core STATIC
  src/core/fdlibm.cpp
  src/core/CMath.cpp
  src/core/CRng.cpp
  src/core/CUtils.cpp
  src/core/CConfig.cpp
  src/core/CTxtLoc.cpp
)
target_include_directories(projecty_core PUBLIC src)
projecty_strict_float(projecty_core)
projecty_warnings(projecty_core)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Threads REQUIRED)
  target_link_libraries(projecty_core PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(PROJECTY_BUILD_TESTS)
  enable_testing()
  add_executable(projecty_tests
    tests/test_main.cpp
    tests/test_fdlibm.cpp
    tests/test_core.cpp
  )
  target_link_libraries(projecty_tests PRIVATE projecty_core)
  projecty_strict_float(projecty_tests)
  add_test(NAME projecty_tests COMMAND projecty_tests WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()

# ---------------------------------------------------------------------------
# Game executable (SDL2 + OpenGL)
# ---------------------------------------------------------------------------
if(PROJECTY_BUILD_GAME)
  find_package(SDL2 CONFIG QUIET)
  if(NOT SDL2_FOUND AND PROJECTY_FETCH_SDL2)
    message(STATUS "SDL2 not found - fetching SDL2 2.30.9 from GitHub")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-2.30.9
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL2)
    set(SDL2_FOUND TRUE)
  endif()

  if(SDL2_FOUND)
    add_executable(ProjectY WIN32
      src/platform/main_sdl.cpp
      src/platform/GLFunctions.cpp
    )
    target_link_libraries(ProjectY PRIVATE projecty_core)
    if(TARGET SDL2::SDL2main)
      target_link_libraries(ProjectY PRIVATE SDL2::SDL2main)
    endif()
    if(TARGET SDL2::SDL2-static)
      target_link_libraries(ProjectY PRIVATE SDL2::SDL2-static)
    else()
      target_link_libraries(ProjectY PRIVATE SDL2::SDL2)
    endif()
    projecty_strict_float(ProjectY)
    projecty_warnings(ProjectY)
  else()
    message(WARNING "SDL2 not available: skipping the game executable (tests still build).")
  endif()
endif()
