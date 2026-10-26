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
    # VS2022: /fp:precise never contracts into FMA (that needs an explicit /fp:contract).
    target_compile_options(${target} PRIVATE /fp:precise)
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
  src/core/CMemoryFile.cpp
  src/core/CVector2.cpp
  src/core/CVector3.cpp
  src/core/CQuaternion.cpp
  src/core/CMatrix3.cpp
  src/core/CMatrix4.cpp
  src/core/CPlane.cpp
)
target_include_directories(projecty_core PUBLIC src)
projecty_strict_float(projecty_core)
projecty_warnings(projecty_core)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(Threads REQUIRED)
  target_link_libraries(projecty_core PUBLIC Threads::Threads)
endif()

# ---------------------------------------------------------------------------
# Detour + DetourTileCache at upstream f6c8d9c (2012-03-10), the version the
# original links, with the game's patches (libm -> fdlibm, 32-bit allocator
# budget). See third_party/recastnavigation/PROJECTY_PATCHES.md.
# ---------------------------------------------------------------------------
add_library(projecty_detour STATIC
  third_party/recastnavigation/Detour/Source/DetourAlloc.cpp
  third_party/recastnavigation/Detour/Source/DetourCommon.cpp
  third_party/recastnavigation/Detour/Source/DetourNavMesh.cpp
  third_party/recastnavigation/Detour/Source/DetourNavMeshBuilder.cpp
  third_party/recastnavigation/Detour/Source/DetourNavMeshQuery.cpp
  third_party/recastnavigation/Detour/Source/DetourNode.cpp
  third_party/recastnavigation/DetourTileCache/Source/DetourTileCache.cpp
  third_party/recastnavigation/DetourTileCache/Source/DetourTileCacheBuilder.cpp
  third_party/recastnavigation/fastlz/fastlz.c
)
target_include_directories(projecty_detour SYSTEM PUBLIC
  third_party/recastnavigation/Detour/Include
  third_party/recastnavigation/DetourTileCache/Include
  third_party/recastnavigation/fastlz)
target_link_libraries(projecty_detour PUBLIC projecty_core)
projecty_strict_float(projecty_detour)
if(MSVC)
  target_compile_definitions(projecty_detour PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# Engine library: rendering + game code (OpenGL entry points are loaded at
# runtime, so this links without an OpenGL SDK)
# ---------------------------------------------------------------------------
file(GLOB PROJECTY_GAME_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/game/*.cpp)
add_library(projecty_engine STATIC
  src/platform/GLFunctions.cpp
  src/render/CTexture.cpp
  src/render/CMesh.cpp
  src/render/CGLSLProgram.cpp
  src/render/CCamera.cpp
  ${PROJECTY_GAME_SOURCES}
)
if(MSVC)
  # the map/script readers use sscanf like the original
  target_compile_definitions(projecty_engine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_include_directories(projecty_engine PUBLIC src)
target_include_directories(projecty_engine SYSTEM PUBLIC third_party)
target_link_libraries(projecty_engine PUBLIC projecty_core projecty_detour)
projecty_strict_float(projecty_engine)
projecty_warnings(projecty_engine)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(PROJECTY_BUILD_TESTS)
  enable_testing()
  file(GLOB PROJECTY_TEST_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/tests/test_*.cpp)
  add_executable(projecty_tests ${PROJECTY_TEST_SOURCES})
  target_link_libraries(projecty_tests PRIVATE projecty_engine)
  projecty_strict_float(projecty_tests)
  add_test(NAME projecty_tests COMMAND projecty_tests WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

  # Dumps a map as the port loads it (also compared against the original
  # engine by tools/emu/check_scripts.py)
  add_executable(level_probe tools/level_probe.cpp)
  target_link_libraries(level_probe PRIVATE projecty_engine)
  projecty_strict_float(level_probe)

  # Steps a skirmish and dumps checksums (compared with tools/emu/game_oracle.py)
  add_executable(sim_probe tools/sim_probe.cpp)
  target_link_libraries(sim_probe PRIVATE projecty_engine)
  projecty_strict_float(sim_probe)

  # Runs a navmesh scenario on the port (compared with tools/emu/nav_oracle.py)
  add_executable(nav_probe tools/nav_probe.cpp)
  target_link_libraries(nav_probe PRIVATE projecty_engine)
  projecty_strict_float(nav_probe)
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
    )
    target_link_libraries(ProjectY PRIVATE projecty_engine)
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
