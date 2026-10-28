#pragma once

#include <cstdint>

#include "core/gfx.h"

namespace zentris::palette {

constexpr uint16_t kInk = rgb(26, 28, 44);
constexpr uint16_t kPaper = rgb(244, 240, 230);
constexpr uint16_t kLabel = rgb(177, 62, 83);
constexpr uint16_t kWhite = rgb(255, 255, 255);
constexpr uint16_t kDropShadow = rgb(20, 10, 32);

struct BlockColors {
  uint16_t base, light, dark;
};

// Indexed by board cell value: 0 empty, 1..7 = PieceType + 1, 8 = game-over curtain.
constexpr int kCurtainBlock = 8;
constexpr BlockColors kBlocks[9] = {
    {0, 0, 0},
    {rgb(60, 224, 232), rgb(170, 248, 252), rgb(20, 130, 150)},   // I
    {rgb(246, 216, 58), rgb(255, 244, 170), rgb(170, 130, 20)},   // O
    {rgb(178, 74, 224), rgb(222, 160, 250), rgb(100, 30, 140)},   // T
    {rgb(74, 192, 74), rgb(170, 236, 150), rgb(30, 110, 40)},     // S
    {rgb(232, 60, 60), rgb(255, 160, 150), rgb(140, 20, 30)},     // Z
    {rgb(60, 90, 232), rgb(150, 180, 255), rgb(25, 40, 140)},     // J
    {rgb(240, 140, 44), rgb(255, 205, 140), rgb(150, 70, 10)},    // L
    {rgb(120, 118, 140), rgb(180, 178, 196), rgb(70, 68, 88)},    // curtain
};

}  // namespace zentris::palette
