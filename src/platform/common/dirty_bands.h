#pragma once

#include <cstdint>

#include "core/gfx.h"

namespace zentris {

// Rows y..y+h-1 of the frame, full width. Contiguous in the framebuffer, so a port can push
// them to the panel straight from it.
struct RowSpan {
  int y, h;
};

// Tells a device port which parts of the frame changed since it last pushed, so it sends
// only those to the panel (a full 320x240 push takes ~33 ms over the ST7789's 37.5 MHz SPI,
// too slow for 60 Hz). The frame is cut into bands of kBandRows rows, and only a hash of
// each band is kept, not its pixels. One band per diff() is sent whether it changed or not,
// round robin, so a hash collision can't leave a stale band on the panel for long.
// Expects a kScreenW x kScreenH framebuffer.
class DirtyBands {
 public:
  static constexpr int kBandRows = 8;
  static constexpr int kBandCount = kScreenH / kBandRows;
  static_assert(kScreenH % kBandRows == 0, "bands must tile the screen");

  // The panel now shows `fb` (it was just pushed whole).
  void mark_shown(const Framebuffer& fb);
  // Forgets what the panel shows: the next diff() returns the whole frame.
  void invalidate() { valid_ = false; }

  // Fills `out` (room for kBandCount) with the spans of `fb` that differ from what the
  // panel shows, top to bottom, adjacent bands merged, and from then on treats them as
  // pushed. Returns how many there are.
  int diff(const Framebuffer& fb, RowSpan* out);

 private:
  uint32_t hashes_[kBandCount] = {};
  bool valid_ = false;
  int refresh_ = 0;  // the band sent next regardless
};

}  // namespace zentris
