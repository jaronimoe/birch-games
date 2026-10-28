#pragma once

#include <array>
#include <cstdint>

namespace zentris {

enum class PieceType : uint8_t { I, O, T, S, Z, J, L };
constexpr int kPieceCount = 7;

struct Cell {
  int8_t x, y;
};

struct Kick {
  int8_t dx, dy;
};

using Shape = std::array<Cell, 4>;

// Cells of a piece inside its bounding box (y grows downward), SRS orientation.
// rot: 0 = spawn, 1 = R (CW), 2 = 180, 3 = L (CCW).
const Shape& piece_cells(PieceType type, int rot);

// SRS wall kicks for rotating from `from_rot` in direction `dir` (+1 CW, -1 CCW).
int kick_count(PieceType type);
Kick kick(PieceType type, int from_rot, int dir, int index);

}  // namespace zentris
