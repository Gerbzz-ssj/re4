# PC port of the RE4 decompilation. See docs/pc-port.md.
#
# This build is separate from the matching GameCube build (configure.py + ninja), which it never
# touches. Configure with port/build.ps1, or by hand from an "x86 Native Tools" prompt:
#
#   cmake -S port -B port/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake_minimum_required(VERSION 3.25)

# Milestone 1 is 32-bit: the decompiled code and the disc formats assume 32-bit pointers.
set(CMAKE_C_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)

project(re4pc C CXX)

option(RE4PC_WITH_AURORA "Link the Aurora GameCube compatibility layer (GX/VI/PAD/OS)" OFF)

set(RE4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- game code -------------------------------------------------------------------------------------
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${RE4_ROOT}/src/game/*.cpp")

# Units with real PowerPC assembly. pc_compat.h would silently drop it, so they stay out until they
# have C replacements (docs/pc-port.md, "Code problems to solve").
set(PC_ASM_UNITS
  dbmodule.cpp   # psq_l
  Espgen42.cpp   # psq_l
  espgen45.cpp   # psq_l
  main.cpp       # li r3 (boot), replaced by port/src/pc_main.cpp
  math_sub.cpp   # frsqrte, fcmpu
  memset_2.cpp   # hand-written memset/memclr/mtxclr
  scheduler.cpp  # li r3 (thread switch)
  shape.cpp      # psq_l / psq_st
  trans.cpp      # psq_l, mtspr GQR
  yz2asm.cpp     # hand-written Yaz0 decoder
)
foreach(unit ${PC_ASM_UNITS})
  list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/game/${unit}$")
endforeach()

add_library(re4_game STATIC ${GAME_SOURCES})

target_include_directories(re4_game PRIVATE
  "${RE4_ROOT}/include"
  "${RE4_ROOT}/src"
)
target_compile_definitions(re4_game PRIVATE
  TARGET_PC=1
  BUILD_VERSION=0
  VERSION_G4BE08
  NDEBUG=1
  _CRT_SECURE_NO_WARNINGS
)
target_compile_options(re4_game PRIVATE
  # The sources test __GNUC__ for the ProDG (GCC) code paths; clang implements those extensions.
  -fgnuc-version=2.95.3
  "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/include/pc_compat.h"
  -fno-strict-aliasing
  -fwrapv
  -fno-exceptions
  -fno-rtti
  -Wno-everything
)

# --- compatibility layer ---------------------------------------------------------------------------
if (RE4PC_WITH_AURORA)
  set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)   # we read an extracted disc folder (port/src/pc_dvd.cpp)
  set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)  # saves go to files (port/src/pc_card.cpp)
  add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)
endif()
