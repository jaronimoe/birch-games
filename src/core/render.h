#pragma once

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"

namespace zentris {

// Draws the whole 320x240 screen for the current game state.
void render(const Game& game, NextWindow& next, Framebuffer& fb);

}  // namespace zentris
