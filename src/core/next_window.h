#pragma once

#include <cstdint>

#include "core/gfx.h"

namespace zentris {

// Whatever lives in the NEXT box. The real next piece doesn't exist yet (it depends on
// where the current one lands), so this slot is free for a gag, a micro game, or pixels.
class NextWindow {
 public:
  virtual ~NextWindow() = default;
  virtual void tick() {}
  virtual void draw(Framebuffer& fb, const Rect& area) = 0;
};

// Placeholder: a bobbing question mark that cycles through the piece colors.
class MysteryNextWindow : public NextWindow {
 public:
  void tick() override { ++frame_; }
  void draw(Framebuffer& fb, const Rect& area) override;

 private:
  uint32_t frame_ = 0;
};

}  // namespace zentris
