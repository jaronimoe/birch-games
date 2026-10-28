#include "bot.h"

#include "core/oracle.h"

namespace zentris::tools {

void GreedyBot::plan(const Game& game) {
  const ActivePiece& p = game.piece();
  const Placement best = best_placement(game.board(), p.type);
  target_rot_ = best.valid() ? best.rot : p.rot;
  target_x_ = best.valid() ? best.x : p.x;
  planned_ = true;
  frames_ = 0;
}

// Releases between presses so every tap registers as a fresh press.
uint16_t GreedyBot::tap(uint16_t action) {
  last_ = (last_ & action) ? 0 : action;
  return last_;
}

uint16_t GreedyBot::input(const Game& game) {
  if (game.state() != GameState::Playing) {
    planned_ = false;
    return tap(kStart);
  }
  if (!game.has_piece() || game.phase() != Phase::Falling) {
    planned_ = false;
    last_ = 0;
    return 0;
  }
  if (reckless_) return tap(kHardDrop);
  if (!planned_) plan(game);

  const ActivePiece& p = game.piece();
  if (++frames_ > 90) return tap(kHardDrop);  // target unreachable, give up on it
  if (p.rot != target_rot_) return tap(kRotateCW);
  if (p.x < target_x_) return tap(kRight);
  if (p.x > target_x_) return tap(kLeft);
  return tap(kHardDrop);
}

}  // namespace zentris::tools
