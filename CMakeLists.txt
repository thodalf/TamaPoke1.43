# IDE-only project file for CLion. The canonical build is tools/emu/build.sh
# (desktop emulator) and arduino-cli (firmware) -- see CLAUDE.md. This just
# mirrors build.sh's g++ invocation as a CMake target so CLion has something
# to index and debug; it does not replace build.sh, and there is nothing here
# for the firmware itself (not a CMake project -- debug that via a GDB Remote
# Debug run configuration against the arduino-cli build instead).
cmake_minimum_required(VERSION 3.16)
project(tamapoke_emu CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(EMU_DIR ${CMAKE_SOURCE_DIR}/tools/emu)

# Not a bare "python3": on Windows that resolves (via PATH, inside Ninja's
# non-interactive cmd.exe) to the Microsoft Store alias stub, which only
# works from an interactive console and fails silently everywhere else.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# SDL2 discovery: a CMake CONFIG package first (vcpkg, or the official
# SDL2-devel-*-mingw/VC zip -- point CMAKE_PREFIX_PATH/SDL2_DIR at it), else
# pkg-config (Homebrew, apt, MSYS2 -- the same thing build.sh's sdl2-config
# reads). Not sdl2-config directly: it's a shell script, and CMake's
# execute_process does not go through a shell, so it can't run one portably.
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
  set(TAMAPOKE_SDL2_TARGET SDL2::SDL2)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2PC QUIET IMPORTED_TARGET sdl2)
  endif()
  if(SDL2PC_FOUND)
    set(TAMAPOKE_SDL2_TARGET PkgConfig::SDL2PC)
  else()
    message(FATAL_ERROR
      "SDL2 not found. Either:\n"
      "  - vcpkg install sdl2, and configure this project with vcpkg's toolchain file, or\n"
      "  - point CMAKE_PREFIX_PATH/SDL2_DIR at an extracted SDL2-devel-*-mingw or -VC package, or\n"
      "  - macOS/Linux: brew install sdl2 / apt install libsdl2-dev (same as tools/emu/build.sh)")
  endif()
endif()

# Mirrors build.sh: g++ can't compile a .ino directly, and the sketch calls
# functions before they're defined, so both proto.h and sketch.cpp are
# generated from TamaPoke.ino before every build.
add_custom_command(
  OUTPUT ${EMU_DIR}/proto.h ${EMU_DIR}/sketch.cpp
  COMMAND ${Python3_EXECUTABLE} ${EMU_DIR}/genproto.py ${CMAKE_SOURCE_DIR}/TamaPoke.ino
  COMMAND ${Python3_EXECUTABLE} ${EMU_DIR}/make_sketch_cpp.py ${CMAKE_SOURCE_DIR}/TamaPoke.ino ${EMU_DIR}/sketch.cpp
  WORKING_DIRECTORY ${EMU_DIR}
  DEPENDS ${CMAKE_SOURCE_DIR}/TamaPoke.ino ${EMU_DIR}/genproto.py ${EMU_DIR}/make_sketch_cpp.py
  COMMENT "Generating sketch.cpp / proto.h from TamaPoke.ino"
  VERBATIM
)
add_custom_target(tamapoke_emu_gen DEPENDS ${EMU_DIR}/proto.h ${EMU_DIR}/sketch.cpp)

add_executable(tamapoke-emu
  ${EMU_DIR}/sketch.cpp
  ${EMU_DIR}/wavout.cpp
  ${CMAKE_SOURCE_DIR}/gbsynth.cpp
  ${CMAKE_SOURCE_DIR}/pet.cpp
  ${CMAKE_SOURCE_DIR}/i18n.cpp
  ${CMAKE_SOURCE_DIR}/party.cpp
  ${CMAKE_SOURCE_DIR}/battle.cpp
  ${CMAKE_SOURCE_DIR}/link.cpp
  ${CMAKE_SOURCE_DIR}/save.cpp
  ${EMU_DIR}/host_impl.cpp
  ${EMU_DIR}/font.cpp
  ${EMU_DIR}/clock.cpp
  ${EMU_DIR}/main_sdl.cpp
)
add_dependencies(tamapoke-emu tamapoke_emu_gen)

target_include_directories(tamapoke-emu PRIVATE ${EMU_DIR} ${CMAKE_SOURCE_DIR})
target_compile_definitions(tamapoke-emu PRIVATE SPRITE_DIR="${CMAKE_SOURCE_DIR}/tools/sdcard/mons")
target_compile_features(tamapoke-emu PRIVATE cxx_std_17)
target_compile_options(tamapoke-emu PRIVATE -w)
# SDL2main first: it wraps main()/WinMain() and calls back into SDL2 symbols,
# so on a static-archive link (MinGW) it must be resolved before SDL2 itself
# or ld never revisits SDL2's archive to satisfy SDL2main's own references.
if(TARGET SDL2::SDL2main)
  target_link_libraries(tamapoke-emu PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(tamapoke-emu PRIVATE ${TAMAPOKE_SDL2_TARGET})

set_target_properties(tamapoke-emu PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${EMU_DIR})
