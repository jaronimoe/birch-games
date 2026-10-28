#include "core/pieces.h"

namespace zentris {
namespace {

constexpr Shape kSpawnShapes[kPieceCount] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}},  // I
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}},  // O
    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}},  // T
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}},  // S
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}},  // Z
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}},  // J
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}},  // L
};

using RotationTable = std::array<std::array<Shape, 4>, kPieceCount>;

// Rotates each spawn shape clockwise inside its SRS bounding box (4x4 for I, 3x3 otherwise).
// O never changes.
constexpr RotationTable build_rotations() {
  RotationTable table{};
  for (int p = 0; p < kPieceCount; ++p) {
    const int box = p == static_cast<int>(PieceType::I) ? 4 : 3;
    table[p][0] = kSpawnShapes[p];
    for (int r = 1; r < 4; ++r) {
      for (int i = 0; i < 4; ++i) {
        const Cell c = table[p][r - 1][i];
        table[p][r][i] = p == static_cast<int>(PieceType::O)
                             ? c
                             : Cell{static_cast<int8_t>(box - 1 - c.y), c.x};
      }
    }
  }
  return table;
}

constexpr RotationTable kRotations = build_rotations();

// SRS kick offsets as published (y grows upward), indexed by the state rotated from.
using KickRow = std::array<Kick, 5>;

constexpr KickRow kJlstzCw[4] = {
    {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},  // 0 -> R
    {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},      // R -> 2
    {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},     // 2 -> L
    {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},   // L -> 0
};
constexpr KickRow kJlstzCcw[4] = {
    {{{0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2}}},     // 0 -> L
    {{{0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2}}},      // R -> 0
    {{{0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2}}},  // 2 -> R
    {{{0, 0}, {-1, 0}, {-1, -1}, {0, 2}, {-1, 2}}},   // L -> 2
};
constexpr KickRow kICw[4] = {
    {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},  // 0 -> R
    {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},  // R -> 2
    {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},  // 2 -> L
    {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},  // L -> 0
};
constexpr KickRow kICcw[4] = {
    {{{0, 0}, {-1, 0}, {2, 0}, {-1, 2}, {2, -1}}},  // 0 -> L
    {{{0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2}}},  // R -> 0
    {{{0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1}}},  // 2 -> R
    {{{0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2}}},  // L -> 2
};

}  // namespace

const Shape& piece_cells(PieceType type, int rot) {
  return kRotations[static_cast<int>(type)][rot & 3];
}

int kick_count(PieceType type) { return type == PieceType::O ? 1 : 5; }

Kick kick(PieceType type, int from_rot, int dir, int index) {
  if (type == PieceType::O) return {0, 0};
  const bool is_i = type == PieceType::I;
  const KickRow* rows = dir > 0 ? (is_i ? kICw : kJlstzCw) : (is_i ? kICcw : kJlstzCcw);
  const Kick k = rows[from_rot & 3][index];
  return {k.dx, static_cast<int8_t>(-k.dy)};
}

}  // namespace zentris
