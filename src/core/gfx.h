#pragma once

#include <cstdint>

namespace zentris {

constexpr int kScreenW = 320;
constexpr int kScreenH = 240;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct Rect {
  int x, y, w, h;
};

// RGB565 pixels in host byte order. The platform layer converts for the panel if needed.
struct Framebuffer {
  uint16_t* px;
  int w, h;

  void set(int x, int y, uint16_t c) {
    if (x >= 0 && y >= 0 && x < w && y < h) px[y * w + x] = c;
  }
  void fill_rect(int x, int y, int rw, int rh, uint16_t c);
};

// 5x7 bitmap font on a 6x8 grid, uppercase only (lowercase is drawn as uppercase).
int text_width(const char* s, int scale = 1);
void draw_text(Framebuffer& fb, int x, int y, const char* s, uint16_t c, int scale = 1);
void draw_text_centered(Framebuffer& fb, int cx, int y, const char* s, uint16_t c, int scale = 1);

}  // namespace zentris
