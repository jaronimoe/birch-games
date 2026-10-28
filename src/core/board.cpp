#include "core/board.h"

#include <cstring>

namespace zentris {

void Board::clear() { std::memset(cells_, 0, sizeof(cells_)); }

bool Board::fits(PieceType type, int rot, int x, int y) const {
  for (const Cell& c : piece_cells(type, rot)) {
    const int cx = x + c.x;
    const int cy = y + c.y;
    if (cx < 0 || cx >= kBoardW || cy < 0 || cy >= kBoardH) return false;
    if (cells_[cy][cx] != 0) return false;
  }
  return true;
}

void Board::place(PieceType type, int rot, int x, int y) {
  const uint8_t value = static_cast<uint8_t>(type) + 1;
  for (const Cell& c : piece_cells(type, rot)) cells_[y + c.y][x + c.x] = value;
}

int Board::find_full_rows(int rows[kMaxClearRows]) const {
  int count = 0;
  for (int y = 0; y < kBoardH && count < kMaxClearRows; ++y) {
    bool full = true;
    for (int x = 0; x < kBoardW && full; ++x) full = cells_[y][x] != 0;
    if (full) rows[count++] = y;
  }
  return count;
}

void Board::remove_rows(const int* rows, int count) {
  int dst = kBoardH - 1;
  int next_removed = count - 1;
  for (int src = kBoardH - 1; src >= 0; --src) {
    if (next_removed >= 0 && rows[next_removed] == src) {
      --next_removed;
      continue;
    }
    if (dst != src) std::memcpy(cells_[dst], cells_[src], kBoardW);
    --dst;
  }
  for (; dst >= 0; --dst) std::memset(cells_[dst], 0, kBoardW);
}

}  // namespace zentris
