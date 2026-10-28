#include "bot.h"

#include <climits>

namespace zentris::tools {
namespace {

// Classic weighted heuristic: reward cleared lines, punish height, holes and bumpiness.
int evaluate(const Board& b, int lines) {
  int heights[kBoardW] = {};
  int holes = 0;
  for (int x = 0; x < kBoardW; ++x) {
    bool found = false;
    for (int y = 0; y < kBoardH; ++y) {
      if (b.at(x, y)) {
        if (!found) heights[x] = kBoardH - y;
        found = true;
      } else if (found) {
        ++holes;
      }
    }
  }
  int aggregate = 0;
  int bumpiness = 0;
  for (int x = 0; x < kBoardW; ++x) {
    aggregate += heights[x];
    if (x + 1 < kBoardW) bumpiness += heights[x] > heights[x + 1] ? heights[x] - heights[x + 1]
                                                                  : heights[x + 1] - heights[x];
  }
  return lines * 760 - aggregate * 51 - holes * 356 - bumpiness * 184;
}

}  // namespace

void GreedyBot::plan(const Game& game) {
  const ActivePiece& p = game.piece();
  const Board& board = game.board();
  int best = INT_MIN;
  target_rot_ = p.rot;
  target_x_ = p.x;
  for (int rot = 0; rot < 4; ++rot) {
    for (int x = -2; x < kBoardW; ++x) {
      if (!board.fits(p.type, rot, x, p.y)) continue;
      int y = p.y;
      while (board.fits(p.type, rot, x, y + 1)) ++y;
      Board after = board;
      after.place(p.type, rot, x, y);
      int rows[kMaxClearRows];
      const int lines = after.find_full_rows(rows);
      after.remove_rows(rows, lines);
      const int score = evaluate(after, lines);
      if (score > best) {
        best = score;
        target_rot_ = rot;
        target_x_ = x;
      }
    }
  }
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
