#include "platform/common/dirty_bands.h"

namespace zentris {

namespace {

// Multiply-rotate over two pixels at a time. The rotate matters: without it, flipping the
// top bit of two words cancels out (a plain xor-multiply never moves it).
uint32_t hash_band(const Framebuffer& fb, int band) {
  const uint16_t* p = fb.px + band * DirtyBands::kBandRows * fb.w;
  const int n = DirtyBands::kBandRows * fb.w;
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < n; i += 2) {
    const uint32_t word = p[i] | (static_cast<uint32_t>(p[i + 1]) << 16);
    h = (((h << 7) | (h >> 25)) ^ word) * 0x9E3779B1u;
  }
  return h ^ (h >> 16);
}

}  // namespace

void DirtyBands::mark_shown(const Framebuffer& fb) {
  for (int b = 0; b < kBandCount; ++b) hashes_[b] = hash_band(fb, b);
  valid_ = true;
}

int DirtyBands::diff(const Framebuffer& fb, RowSpan* out) {
  int count = 0;
  for (int b = 0; b < kBandCount; ++b) {
    const uint32_t h = hash_band(fb, b);
    const bool dirty = !valid_ || h != hashes_[b] || b == refresh_;
    hashes_[b] = h;
    if (!dirty) continue;
    if (count > 0 && out[count - 1].y + out[count - 1].h == b * kBandRows) {
      out[count - 1].h += kBandRows;
    } else {
      out[count++] = {b * kBandRows, kBandRows};
    }
  }
  valid_ = true;
  refresh_ = (refresh_ + 1) % kBandCount;
  return count;
}

}  // namespace zentris
