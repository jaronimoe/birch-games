#pragma once

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/oracle_speech.h"

namespace zentris {

struct RenderOptions {
  bool show_oracle_target = false;  // debug: outline where the oracle meant the piece to go
};

// Draws the whole 320x240 screen for the current game state.
void render(const Game& game, NextWindow& next, const OracleSpeech& speech, Framebuffer& fb,
            const RenderOptions& options = {});

}  // namespace zentris
