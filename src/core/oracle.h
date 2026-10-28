#pragma once

#include <climits>
#include <cstdint>

#include "core/board.h"
#include "core/pieces.h"

namespace zentris {

// Where a piece ends up when dropped straight down, and how good that is.
struct Placement {
  int rot = 0, x = 0, y = 0;
  int lines = 0;
  int score = INT_MIN;  // INT_MIN: no reachable placement

  bool valid() const { return score != INT_MIN; }
};

struct OracleChoice {
  PieceType type;
  Placement target;  // the spot the oracle had in mind (kept secret from the player)
};

// Board heuristic: rewards cleared lines, punishes stack height, holes and bumpiness.
int evaluate_board(const Board& board, int lines);

// Best straight-drop placement for `type`, reached from the spawn position by rotating and
// then sliding along the spawn row. Most lines cleared wins, then the heuristic.
Placement best_placement(const Board& board, PieceType type);

// Picks the piece the player needs: the one that clears the most lines. If no piece can
// clear a line, picks among the pieces that leave the board in the best shape.
// `random_bits` breaks ties.
OracleChoice choose_piece(const Board& board, uint32_t random_bits);

}  // namespace zentris
