#include "core/next_window.h"

#include "core/palette.h"

namespace zentris {

void MysteryNextWindow::draw(Framebuffer& fb, const Rect& area) {
  constexpr int kScale = 3;
  const int color = 1 + static_cast<int>((frame_ / 30) % 7);
  const int phase = static_cast<int>(frame_ / 8) % 8;
  const int bob = phase < 4 ? phase : 8 - phase;  // 0,1,2,3,4,3,2,1
  const int y = area.y + (area.h - 7 * kScale) / 2 - 2 + bob;
  const int cx = area.x + area.w / 2;
  draw_text_centered(fb, cx + 1, y + 1, "?", palette::kBlocks[color].dark, kScale);
  draw_text_centered(fb, cx, y, "?", palette::kBlocks[color].base, kScale);
}

}  // namespace zentris
