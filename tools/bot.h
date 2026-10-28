#pragma once

#include <cstdint>

#include "core/game.h"

namespace zentris::tools {

// Plays the game through the same Action inputs a player would use. Used by tests and
// the screenshot tool to drive the core without a window.
class GreedyBot {
 public:
  // Reckless: hard-drop every piece where it spawns (tops out fast).
  void set_reckless(bool reckless) { reckless_ = reckless; }
  uint16_t input(const Game& game);

 private:
  void plan(const Game& game);
  uint16_t tap(uint16_t action);

  bool reckless_ = false;
  bool planned_ = false;
  int target_rot_ = 0;
  int target_x_ = 0;
  int frames_ = 0;
  uint16_t last_ = 0;
};

}  // namespace zentris::tools
