#include "core/oracle.h"

namespace zentris {
namespace {

// Without a line to clear, pieces whose best result is this close to the very best one
// count as equally good, so the fallback doesn't always deal the same flat-stack piece.
// Kept below one hole's penalty: a piece that must leave a hole never ties with a clean one.
constexpr int kFallbackMargin = 250;

bool reachable(const Board& board, PieceType type, int rot, int x) {
  const int step = x < kSpawnX ? -1 : 1;
  for (int cx = kSpawnX;; cx += step) {
    if (!board.fits(type, rot, cx, kSpawnY)) return false;
    if (cx == x) return true;
  }
}

}  // namespace

int evaluate_board(const Board& board, int lines) {
  int heights[kBoardW] = {};
  int holes = 0;
  for (int x = 0; x < kBoardW; ++x) {
    bool found = false;
    for (int y = 0; y < kBoardH; ++y) {
      if (board.at(x, y)) {
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
    if (x + 1 < kBoardW) {
      const int d = heights[x] - heights[x + 1];
      bumpiness += d < 0 ? -d : d;
    }
  }
  return lines * 760 - aggregate * 51 - holes * 356 - bumpiness * 184;
}

Placement best_placement(const Board& board, PieceType type) {
  Placement best;
  const int rotations = type == PieceType::O ? 1 : 4;
  for (int rot = 0; rot < rotations; ++rot) {
    for (int x = -2; x < kBoardW; ++x) {
      if (!reachable(board, type, rot, x)) continue;
      int y = kSpawnY;
      while (board.fits(type, rot, x, y + 1)) ++y;

      Board after = board;
      after.place(type, rot, x, y);
      int rows[kMaxClearRows];
      const int lines = after.find_full_rows(rows);
      after.remove_rows(rows, lines);
      const int score = evaluate_board(after, lines);

      if (lines > best.lines || (lines == best.lines && score > best.score))
        best = {rot, x, y, lines, score};
    }
  }
  return best;
}

OracleChoice choose_piece(const Board& board, uint32_t random_bits) {
  Placement best[kPieceCount];
  int max_lines = 0;
  int top_score = INT_MIN;
  for (int t = 0; t < kPieceCount; ++t) {
    best[t] = best_placement(board, static_cast<PieceType>(t));
    if (!best[t].valid()) continue;
    if (best[t].lines > max_lines) max_lines = best[t].lines;
    if (best[t].score > top_score) top_score = best[t].score;
  }

  PieceType candidates[kPieceCount];
  int count = 0;
  for (int t = 0; t < kPieceCount; ++t) {
    if (!best[t].valid()) continue;
    const bool wanted = max_lines > 0 ? best[t].lines == max_lines
                                      : best[t].score >= top_score - kFallbackMargin;
    if (wanted) candidates[count++] = static_cast<PieceType>(t);
  }

  // Nothing fits anywhere: hand out anything, the spawn will top out.
  if (count == 0) return {PieceType::I, Placement{}};

  const PieceType type = candidates[random_bits % static_cast<uint32_t>(count)];
  return {type, best[static_cast<int>(type)]};
}

}  // namespace zentris
