cmake_minimum_required(VERSION 3.20)
project(zentris LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Under emcmake only the browser player is built; natively everything but the player.
# Pulled into another build (birch's firmware, add_subdirectory) or cross-compiled, only the
# libraries are.
if(EMSCRIPTEN OR CMAKE_CROSSCOMPILING OR NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(zentris_native OFF)
else()
  set(zentris_native ON)
endif()
option(ZENTRIS_BUILD_SIM "Build the SDL3 desktop simulator" ${zentris_native})
option(ZENTRIS_BUILD_TOOLS "Build tests and the screenshot tool" ${zentris_native})

# Hardware-independent game core, shared by the simulator, the browser and the device.
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

# Hardware-free helpers for the device port (input mapping, dirty-band pushes, records blob),
# tested on the host.
add_library(zentris_platform_common STATIC
  src/platform/common/dirty_bands.cpp
  src/platform/common/pad_input.cpp
)
target_link_libraries(zentris_platform_common PUBLIC zentris_core)
target_compile_options(zentris_platform_common PRIVATE -Wall -Wextra -Wpedantic)

if(ZENTRIS_BUILD_SIM)
  find_package(SDL3 REQUIRED CONFIG)
  add_executable(zentris_sim src/platform/sdl/main.cpp)
  target_link_libraries(zentris_sim PRIVATE zentris_core SDL3::SDL3)
  target_compile_options(zentris_sim PRIVATE -Wall -Wextra)
endif()

if(EMSCRIPTEN)
  # Static site in <build>/web: zentris.js + zentris.wasm next to the page files.
  set(web_out ${CMAKE_BINARY_DIR}/web)
  add_executable(zentris_web src/platform/web/main.cpp)
  target_link_libraries(zentris_web PRIVATE zentris_core)
  target_compile_options(zentris_web PRIVATE -Wall -Wextra)
  set_target_properties(zentris_web PROPERTIES OUTPUT_NAME zentris RUNTIME_OUTPUT_DIRECTORY ${web_out})
  target_link_options(zentris_web PRIVATE
    -sENVIRONMENT=web
    -sMODULARIZE=1
    -sEXPORT_NAME=createZentris
    -sEXPORTED_RUNTIME_METHODS=HEAPU8
  )
  foreach(page_file index.html player.css player.js)
    configure_file(web/${page_file} ${web_out}/${page_file} COPYONLY)
  endforeach()
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

  add_executable(zentris_platform_tests tests/platform_tests.cpp)
  target_link_libraries(zentris_platform_tests PRIVATE zentris_bot zentris_platform_common)
  target_compile_options(zentris_platform_tests PRIVATE -Wall -Wextra)

  enable_testing()
  add_test(NAME core_tests COMMAND zentris_tests)
  add_test(NAME platform_tests COMMAND zentris_platform_tests)
endif()
