#pragma once

#include "ui.h"

// ZENTRIS as a game on the birch console (birch's CONSOLE.md, "Adding a game"). Built into
// birch's firmware, against birch's headers: birch's CMakeLists.txt compiles this directory
// and links zentris_core.

// Call once at boot. Loads the saved records and returns the game's entry screen for the
// console's list.
const UiScreen *zentris_game_init();

// Console factory reset: forgets the records (the flash copy is already erased by then).
void zentris_game_factory_reset();
