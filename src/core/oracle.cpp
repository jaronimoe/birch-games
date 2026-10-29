#include "core/oracle.h"

namespace zentris {
namespace {

// When lines can be cleared, equally ranked pieces only tie if their heuristic is this close
// to the best one, which weeds out clears that leave a tower. Kept below one hole's penalty.
constexpr int kClearTieMargin = 250;

bool reachable(const Board& board, PieceType type, int rot, int x) {
  const int step = x < kSpawnX ? -1 : 1;
  for (int cx = kSpawnX;; cx += step) {
    if (!board.fits(type, rot, cx, kSpawnY)) return false;
    if (cx == x) return true;
  }
}

// Ranks by every criterion except the heuristic score.
bool same_rank(const Placement& a, const Placement& b) {
  return a.lines == b.lines && a.new_holes == b.new_holes && a.deepest == b.deepest;
}

bool better(const Placement& a, const Placement& b) {
  if (a.lines != b.lines) return a.lines > b.lines;
  if (a.new_holes != b.new_holes) return a.new_holes < b.new_holes;
  if (a.deepest != b.deepest) return a.deepest > b.deepest;
  return a.score > b.score;
}

}  // namespace

int count_holes(const Board& board) {
  int holes = 0;
  for (int x = 0; x < kBoardW; ++x) {
    bool covered = false;
    for (int y = 0; y < kBoardH; ++y) {
      if (board.at(x, y)) covered = true;
      else if (covered) ++holes;
    }
  }
  return holes;
}

int evaluate_board(const Board& board, int lines) {
  int heights[kBoardW] = {};
  for (int x = 0; x < kBoardW; ++x) {
    for (int y = 0; y < kBoardH; ++y) {
      if (board.at(x, y)) {
        heights[x] = kBoardH - y;
        break;
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
  return lines * 760 - aggregate * 51 - count_holes(board) * 356 - bumpiness * 184;
}

Placement evaluate_placement(const Board& board, PieceType type, int rot, int x, int y,
                             int holes_before) {
  Placement p{rot, x, y};
  for (const Cell& c : piece_cells(type, rot))
    if (y + c.y > p.deepest) p.deepest = y + c.y;

  Board after = board;
  after.place(type, rot, x, y);
  int rows[kMaxClearRows];
  p.lines = after.find_full_rows(rows);
  after.remove_rows(rows, p.lines);
  const int holes = count_holes(after) - holes_before;
  p.new_holes = holes > 0 ? holes : 0;
  p.score = evaluate_board(after, p.lines);
  return p;
}

Placement best_placement(const Board& board, PieceType type) {
  const int holes_before = count_holes(board);
  Placement best;
  const int rotations = type == PieceType::O ? 1 : 4;
  for (int rot = 0; rot < rotations; ++rot) {
    for (int x = -2; x < kBoardW; ++x) {
      if (!reachable(board, type, rot, x)) continue;
      int y = kSpawnY;
      while (board.fits(type, rot, x, y + 1)) ++y;
      const Placement p = evaluate_placement(board, type, rot, x, y, holes_before);
      if (!best.valid() || better(p, best)) best = p;
    }
  }
  return best;
}

OracleChoice choose_piece(const Board& board, uint32_t random_bits) {
  Placement best[kPieceCount];
  int top = -1;
  for (int t = 0; t < kPieceCount; ++t) {
    best[t] = best_placement(board, static_cast<PieceType>(t));
    if (best[t].valid() && (top < 0 || better(best[t], best[top]))) top = t;
  }

  // Nothing fits anywhere: hand out anything, the spawn will top out.
  if (top < 0) return {PieceType::I, Placement{}};

  // Without a clear, every piece that fills the lowest gap equally cleanly is fair game.
  const int min_score = best[top].lines > 0 ? best[top].score - kClearTieMargin : INT_MIN;
  PieceType candidates[kPieceCount];
  int count = 0;
  for (int t = 0; t < kPieceCount; ++t) {
    if (best[t].valid() && same_rank(best[t], best[top]) && best[t].score >= min_score)
      candidates[count++] = static_cast<PieceType>(t);
  }

  const PieceType type = candidates[random_bits % static_cast<uint32_t>(count)];
  return {type, best[static_cast<int>(type)]};
}

}  // namespace zentris
