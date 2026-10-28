#include "core/render.h"

#include "core/palette.h"

namespace zentris {
namespace {

using namespace palette;

// Layout (landscape 320x240): well on the left, stat boxes in the middle, scenery on the right.
constexpr int kCell = 11;  // 10px block + 1px gap
constexpr Rect kWell{20, 9, 1 + kBoardW * kCell, 1 + kVisibleRows * kCell};
constexpr Rect kNextBox{146, 5, 100, 66};
constexpr Rect kStatBoxes[4] = {
    {146, 79, 100, 32}, {146, 119, 100, 32}, {146, 159, 100, 32}, {146, 199, 100, 32}};

constexpr uint16_t kWellBg = rgb(13, 11, 26);
constexpr uint16_t kWellFlash = rgb(70, 60, 110);
constexpr uint16_t kGridDot = rgb(34, 31, 58);
constexpr uint16_t kWellFrame[4] = {rgb(60, 58, 80), rgb(150, 148, 170), rgb(205, 203, 220),
                                    rgb(90, 88, 110)};

constexpr uint16_t kSky[6] = {rgb(27, 16, 53),  rgb(59, 31, 92),  rgb(107, 47, 110),
                              rgb(168, 67, 106), rgb(224, 112, 90), rgb(244, 169, 94)};
constexpr uint16_t kStar = rgb(255, 244, 214);
constexpr uint16_t kStarDim = rgb(150, 130, 170);
constexpr uint16_t kMoon = rgb(251, 233, 183);
constexpr uint16_t kMoonHalo = rgb(120, 70, 130);
constexpr uint16_t kFarHills = rgb(90, 45, 106);
constexpr uint16_t kNearHills = rgb(43, 22, 64);
constexpr uint16_t kPagoda = rgb(20, 10, 32);
constexpr uint16_t kLantern = rgb(255, 211, 107);

constexpr uint8_t kBayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

// Triangle wave in [0, amp] for retro hill silhouettes.
int tri(int x, int period, int amp) {
  const int m = ((x % period) + period) % period;
  const int d = 2 * m - period;
  return amp * (d < 0 ? -d : d) / period;
}

void draw_pagoda(Framebuffer& fb, int cx, int ground) {
  int y = ground - 4;
  fb.fill_rect(cx - 20, y, 40, 4, kPagoda);
  for (int tier = 0; tier < 4; ++tier) {
    const int bw = 22 - tier * 4;
    const int bh = 13 - tier;
    y -= bh;
    fb.fill_rect(cx - bw / 2, y, bw, bh, kPagoda);
    fb.fill_rect(cx - 1, y + 4, 2, 4, kLantern);
    if (tier < 2) {
      fb.fill_rect(cx - bw / 2 + 3, y + 4, 2, 4, kLantern);
      fb.fill_rect(cx + bw / 2 - 5, y + 4, 2, 4, kLantern);
    }
    const int rw = bw + 16;
    fb.fill_rect(cx - rw / 2, y - 2, rw, 2, kPagoda);
    fb.fill_rect(cx - rw / 2 + 3, y - 4, rw - 6, 2, kPagoda);
    fb.set(cx - rw / 2 - 1, y - 3, kPagoda);  // upturned eaves
    fb.set(cx + rw / 2, y - 3, kPagoda);
    y -= 4;
  }
  fb.fill_rect(cx - 1, y - 12, 2, 12, kPagoda);
  fb.fill_rect(cx - 3, y - 5, 6, 1, kPagoda);
  fb.fill_rect(cx - 2, y - 9, 4, 1, kPagoda);
}

void draw_background(Framebuffer& fb, uint32_t frame) {
  for (int y = 0; y < fb.h; ++y) {
    const int v = y >= 170 ? 80 : y * 80 / 170;  // dithered dusk gradient, done by the horizon
    uint16_t* row = fb.px + y * fb.w;
    for (int x = 0; x < fb.w; ++x) {
      row[x] = kSky[(v + kBayer[y & 3][x & 3]) / 16];  // v <= 80, so the index stays <= 5
      if (y < 100) {
        const uint32_t h = static_cast<uint32_t>(x) * 7919u ^ static_cast<uint32_t>(y) * 104729u;
        if (h % 211u == 0) row[x] = ((h >> 9) + frame / 20) % 7 == 0 ? kStarDim : kStar;
      }
    }
  }

  constexpr int kMoonX = 284, kMoonY = 32, kMoonR = 12, kHaloR = 17;
  for (int y = kMoonY - kHaloR; y <= kMoonY + kHaloR; ++y) {
    for (int x = kMoonX - kHaloR; x <= kMoonX + kHaloR; ++x) {
      const int d2 = (x - kMoonX) * (x - kMoonX) + (y - kMoonY) * (y - kMoonY);
      if (d2 <= kMoonR * kMoonR) fb.set(x, y, kMoon);
      else if (d2 <= kHaloR * kHaloR && ((x + y) & 1)) fb.set(x, y, kMoonHalo);
    }
  }

  for (int x = 0; x < fb.w; ++x) {
    const int far_top = 150 - tri(x, 96, 34) - tri(x + 20, 41, 9);
    fb.fill_rect(x, far_top, 1, fb.h - far_top, kFarHills);
    const int near_top = 200 - tri(x + 50, 70, 22) - tri(x + 5, 23, 6);
    fb.fill_rect(x, near_top, 1, fb.h - near_top, kNearHills);
  }

  draw_pagoda(fb, 284, 236);
}

void draw_box(Framebuffer& fb, const Rect& r) {
  fb.fill_rect(r.x + 2, r.y + 2, r.w, r.h, kDropShadow);
  fb.fill_rect(r.x, r.y, r.w, r.h, kInk);
  fb.fill_rect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, kPaper);
}

// Writes `value` as decimal, zero-padded to `digits`.
void format_number(char* out, uint32_t value, int digits) {
  char tmp[12];
  int n = 0;
  do {
    tmp[n++] = static_cast<char>('0' + value % 10);
    value /= 10;
  } while (value && n < 11);
  while (n < digits) tmp[n++] = '0';
  for (int i = 0; i < n; ++i) out[i] = tmp[n - 1 - i];
  out[n] = '\0';
}

void draw_stat(Framebuffer& fb, const Rect& r, const char* label, uint32_t value, int digits) {
  char buf[12];
  format_number(buf, value, digits);
  draw_box(fb, r);
  draw_text_centered(fb, r.x + r.w / 2, r.y + 6, label, kLabel);
  draw_text_centered(fb, r.x + r.w / 2, r.y + 18, buf, kInk);
}

void draw_block(Framebuffer& fb, int px, int py, int value) {
  const BlockColors& c = kBlocks[value];
  fb.fill_rect(px, py, 10, 10, c.base);
  fb.fill_rect(px, py, 10, 1, c.light);
  fb.fill_rect(px, py, 1, 10, c.light);
  fb.fill_rect(px + 1, py + 9, 9, 1, c.dark);
  fb.fill_rect(px + 9, py + 1, 1, 9, c.dark);
  fb.fill_rect(px + 1, py + 1, 2, 1, kWhite);
  fb.set(px + 1, py + 2, kWhite);
}

int cell_px(int x) { return kWell.x + 1 + x * kCell; }
int cell_py(int board_y) { return kWell.y + 1 + (board_y - kHiddenRows) * kCell; }

bool cleared_by_wipe(const Game& game, int x, int y) {
  if (!game.is_clearing_row(y)) return false;
  const int step = game.line_clear_step();  // wipe spreads outward from the middle
  return x >= 4 - step && x <= 5 + step;
}

void draw_well_frame(Framebuffer& fb, bool flash) {
  for (int i = 0; i < 4; ++i) {
    const int inset = 4 - i;
    fb.fill_rect(kWell.x - inset, kWell.y - inset, kWell.w + 2 * inset, kWell.h + 2 * inset,
                 kWellFrame[i]);
  }
  fb.fill_rect(kWell.x, kWell.y, kWell.w, kWell.h, flash ? kWellFlash : kWellBg);
}

void draw_panel(Framebuffer& fb, int y, int h) {
  fb.fill_rect(kWell.x + 6, y, kWell.w - 12, h, kInk);
  fb.fill_rect(kWell.x + 8, y + 2, kWell.w - 16, h - 4, kPaper);
}

void draw_title(Framebuffer& fb, const Game& game) {
  const char* kName = "ZENTRIS";
  const int scale = 2;
  int x = kWell.x + kWell.w / 2 - text_width(kName, scale) / 2;
  for (int i = 0; kName[i]; ++i, x += 6 * scale) {
    const char letter[2] = {kName[i], '\0'};
    draw_text(fb, x + 1, 61, letter, kBlocks[i + 1].dark, scale);
    draw_text(fb, x, 60, letter, kBlocks[i + 1].base, scale);
  }
  const int cx = kWell.x + kWell.w / 2;
  draw_text_centered(fb, cx, 96, "THE UNIVERSE", kPaper);
  draw_text_centered(fb, cx, 106, "PROVIDES", kPaper);
  if ((game.frame() / 30) % 2 == 0) draw_text_centered(fb, cx, 170, "PRESS START", kBlocks[2].base);
}

void draw_game_over(Framebuffer& fb, const Game& game) {
  const int cx = kWell.x + kWell.w / 2;
  draw_panel(fb, 76, 70);
  draw_text_centered(fb, cx, 86, "GAME OVER", kLabel);
  char buf[12];
  format_number(buf, game.score(), 6);
  draw_text_centered(fb, cx, 102, buf, kInk);
  if (game.new_hi_score()) draw_text_centered(fb, cx, 114, "NEW BEST!", kBlocks[7].dark);
  if ((game.frame() / 30) % 2 == 0) draw_text_centered(fb, cx, 130, "PRESS START", kInk);
}

void draw_oracle_target(Framebuffer& fb, const Game& game) {
  const Placement& t = game.oracle_target();
  if (!t.valid()) return;
  const uint16_t c = kBlocks[static_cast<int>(game.piece().type) + 1].light;
  for (const Cell& cell : piece_cells(game.piece().type, t.rot)) {
    const int y = t.y + cell.y;
    if (y < kHiddenRows) continue;
    const int px = cell_px(t.x + cell.x);
    const int py = cell_py(y);
    fb.fill_rect(px, py, 10, 1, c);
    fb.fill_rect(px, py + 9, 10, 1, c);
    fb.fill_rect(px, py, 1, 10, c);
    fb.fill_rect(px + 9, py, 1, 10, c);
  }
}

void draw_well(Framebuffer& fb, const Game& game, const RenderOptions& options) {
  const bool flash = game.phase() == Phase::LineClear && game.clear_count() == 4 &&
                     game.state() == GameState::Playing && (game.line_clear_step() & 1);
  draw_well_frame(fb, flash);

  switch (game.state()) {
    case GameState::Title:
      draw_title(fb, game);
      return;
    case GameState::Paused:
      draw_text_centered(fb, kWell.x + kWell.w / 2, 110, "PAUSE", kPaper);
      return;
    case GameState::Playing:
    case GameState::GameOver:
      break;
  }

  const Board& board = game.board();
  for (int y = kHiddenRows; y < kBoardH; ++y) {
    for (int x = 0; x < kBoardW; ++x) {
      const int v = board.at(x, y);
      if (v && !cleared_by_wipe(game, x, y)) draw_block(fb, cell_px(x), cell_py(y), v);
      else fb.fill_rect(cell_px(x) + 4, cell_py(y) + 4, 2, 2, kGridDot);
    }
  }

  if (game.has_piece() && options.show_oracle_target) draw_oracle_target(fb, game);

  if (game.has_piece()) {
    const ActivePiece& p = game.piece();
    for (const Cell& c : piece_cells(p.type, p.rot)) {
      const int y = p.y + c.y;
      if (y >= kHiddenRows)
        draw_block(fb, cell_px(p.x + c.x), cell_py(y), static_cast<int>(p.type) + 1);
    }
  }

  if (game.state() == GameState::GameOver) {
    for (int row = 0; row < game.curtain_rows(); ++row)
      for (int x = 0; x < kBoardW; ++x)
        draw_block(fb, cell_px(x), cell_py(row + kHiddenRows), kCurtainBlock);
    if (game.game_over_ready()) draw_game_over(fb, game);
  }
}

}  // namespace

void render(const Game& game, NextWindow& next, Framebuffer& fb, const RenderOptions& options) {
  draw_background(fb, game.frame());
  draw_well(fb, game, options);

  draw_box(fb, kNextBox);
  draw_text_centered(fb, kNextBox.x + kNextBox.w / 2, kNextBox.y + 6, "NEXT", kLabel);
  next.draw(fb, {kNextBox.x + 6, kNextBox.y + 16, kNextBox.w - 12, kNextBox.h - 20});

  draw_stat(fb, kStatBoxes[0], "SCORE", game.score(), 6);
  draw_stat(fb, kStatBoxes[1], "TOP", game.hi_score(), 6);
  draw_stat(fb, kStatBoxes[2], "LEVEL", static_cast<uint32_t>(game.level()), 2);
  draw_stat(fb, kStatBoxes[3], "LINES", static_cast<uint32_t>(game.lines()), 3);
}

}  // namespace zentris
