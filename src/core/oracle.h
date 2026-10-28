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
  int new_holes = 0;    // empty cells newly covered (after any cleared lines are removed)
  int deepest = -1;     // lowest board row the piece reaches; larger is deeper
  int score = INT_MIN;  // board heuristic; INT_MIN: no reachable placement

  bool valid() const { return score != INT_MIN; }
};

struct OracleChoice {
  PieceType type;
  Placement target;  // the spot the oracle had in mind (kept secret from the player)
};

int count_holes(const Board& board);

// Board heuristic: rewards cleared lines, punishes stack height, holes and bumpiness.
int evaluate_board(const Board& board, int lines);

// Best straight-drop placement for `type`, reached from the spawn position by rotating and
// then sliding along the spawn row. Ranked by: most lines cleared, fewest new holes,
// deepest reach (fills the lowest gap), then the heuristic.
Placement best_placement(const Board& board, PieceType type);

// Picks the piece the player needs: the one whose best placement ranks highest, i.e. it
// clears the most lines, or failing that fills the lowest gap without creating a hole.
// `random_bits` picks among equally ranked pieces; for line clears, only those whose
// heuristic is close to the best count as equal.
OracleChoice choose_piece(const Board& board, uint32_t random_bits);

}  // namespace zentris
