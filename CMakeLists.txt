cmake_minimum_required(VERSION 3.20)
project(zentris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ZENTRIS_BUILD_SIM "Build the SDL3 desktop simulator" ON)
option(ZENTRIS_BUILD_TOOLS "Build tests and the screenshot tool" ON)

# Hardware-independent game core, shared by the simulator and (later) the Pico build.
add_library(zentris_core STATIC
  src/core/board.cpp
  src/core/game.cpp
  src/core/gfx.cpp
  src/core/next_window.cpp
  src/core/oracle.cpp
  src/core/oracle_lines.cpp
  src/core/oracle_speech.cpp
  src/core/pieces.cpp
  src/core/render.cpp
)
target_include_directories(zentris_core PUBLIC src)
target_compile_options(zentris_core PRIVATE -Wall -Wextra -Wpedantic)

if(ZENTRIS_BUILD_SIM)
  find_package(SDL3 REQUIRED CONFIG)
  add_executable(zentris_sim src/platform/sdl/main.cpp)
  target_link_libraries(zentris_sim PRIVATE zentris_core SDL3::SDL3)
  target_compile_options(zentris_sim PRIVATE -Wall -Wextra)
endif()

if(ZENTRIS_BUILD_TOOLS)
  add_library(zentris_bot STATIC tools/bot.cpp)
  target_link_libraries(zentris_bot PUBLIC zentris_core)
  target_include_directories(zentris_bot PUBLIC tools)

  add_executable(zentris_tests tests/core_tests.cpp)
  target_link_libraries(zentris_tests PRIVATE zentris_bot)
  target_compile_options(zentris_tests PRIVATE -Wall -Wextra)

  add_executable(zentris_shot tools/screenshot.cpp)
  target_link_libraries(zentris_shot PRIVATE zentris_bot)

  enable_testing()
  add_test(NAME core_tests COMMAND zentris_tests)
endif()
