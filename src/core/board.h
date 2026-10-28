#pragma once

#include <cstdint>

#include "core/pieces.h"

namespace zentris {

constexpr int kBoardW = 10;
constexpr int kVisibleRows = 20;
constexpr int kHiddenRows = 2;  // buffer above the visible well for rotations near the top
constexpr int kBoardH = kVisibleRows + kHiddenRows;
constexpr int kMaxClearRows = 4;
constexpr int kSpawnX = 3;
constexpr int kSpawnY = kHiddenRows;  // the top of the spawn box sits on the first visible row

// Cell value 0 is empty; otherwise it is the PieceType + 1 that left the block there.
class Board {
 public:
  Board() { clear(); }

  void clear();
  uint8_t at(int x, int y) const { return cells_[y][x]; }
  void set(int x, int y, uint8_t value) { cells_[y][x] = value; }

  bool fits(PieceType type, int rot, int x, int y) const;
  void place(PieceType type, int rot, int x, int y);

  // Writes the indices of full rows (top to bottom) into `rows`, returns how many.
  int find_full_rows(int rows[kMaxClearRows]) const;
  // Removes the given rows (sorted top to bottom) and drops everything above them.
  void remove_rows(const int* rows, int count);

 private:
  uint8_t cells_[kBoardH][kBoardW];
};

}  // namespace zentris
