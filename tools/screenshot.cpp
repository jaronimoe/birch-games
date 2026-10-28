// Renders a few game situations to BMP files without a window, using the bot as player.
// Usage: zentris_shot [out_dir]   (default: shots)

#include <cstdio>
#include <string>

#include "bot.h"
#include "core/game.h"
#include "core/next_window.h"
#include "core/render.h"

using namespace zentris;

namespace {

constexpr int kScale = 2;

void put_u32(std::FILE* f, uint32_t v) {
  const unsigned char b[4] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8),
                              static_cast<unsigned char>(v >> 16),
                              static_cast<unsigned char>(v >> 24)};
  std::fwrite(b, 1, 4, f);
}

void put_u16(std::FILE* f, uint16_t v) {
  const unsigned char b[2] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8)};
  std::fwrite(b, 1, 2, f);
}

// 24-bit BMP, upscaled by kScale. Rows are stored bottom-up.
bool write_bmp(const std::string& path, const Framebuffer& fb) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const int w = fb.w * kScale;
  const int h = fb.h * kScale;
  const uint32_t row_bytes = static_cast<uint32_t>((w * 3 + 3) & ~3);
  const uint32_t data_size = row_bytes * static_cast<uint32_t>(h);

  std::fwrite("BM", 1, 2, f);
  put_u32(f, 54 + data_size);
  put_u32(f, 0);
  put_u32(f, 54);
  put_u32(f, 40);
  put_u32(f, static_cast<uint32_t>(w));
  put_u32(f, static_cast<uint32_t>(h));
  put_u16(f, 1);
  put_u16(f, 24);
  put_u32(f, 0);
  put_u32(f, data_size);
  put_u32(f, 2835);
  put_u32(f, 2835);
  put_u32(f, 0);
  put_u32(f, 0);

  std::string row(row_bytes, '\0');
  for (int y = h - 1; y >= 0; --y) {
    for (int x = 0; x < w; ++x) {
      const uint16_t c = fb.px[(y / kScale) * fb.w + x / kScale];
      const int r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
      row[x * 3 + 0] = static_cast<char>((b << 3) | (b >> 2));
      row[x * 3 + 1] = static_cast<char>((g << 2) | (g >> 4));
      row[x * 3 + 2] = static_cast<char>((r << 3) | (r >> 2));
    }
    std::fwrite(row.data(), 1, row_bytes, f);
  }
  std::fclose(f);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : "shots";
  static uint16_t pixels[kScreenW * kScreenH];
  Framebuffer fb{pixels, kScreenW, kScreenH};
  Game game(3);
  MysteryNextWindow next;
  tools::GreedyBot bot;

  auto step = [&](uint16_t input) {
    game.tick(input);
    next.tick();
  };
  auto shoot = [&](const char* name, const RenderOptions& options = {}) {
    render(game, next, fb, options);
    const std::string path = dir + "/" + name;
    if (write_bmp(path, fb)) std::printf("wrote %s\n", path.c_str());
    else std::printf("could not write %s (does the directory exist?)\n", path.c_str());
  };

  for (int i = 0; i < 30; ++i) step(0);
  shoot("title.bmp");

  int guard = 0;
  while (!(game.lines() >= 12 && game.has_piece() && game.piece().y >= kHiddenRows + 4) &&
         ++guard < 50000)
    step(bot.input(game));
  shoot("play.bmp");
  RenderOptions debug;
  debug.show_oracle_target = true;
  shoot("oracle.bmp", debug);

  guard = 0;
  while (!(game.phase() == Phase::LineClear && game.line_clear_step() == 2) && ++guard < 50000)
    step(bot.input(game));
  shoot("clear.bmp");

  bot.set_reckless(true);
  guard = 0;
  while (game.state() != GameState::GameOver && ++guard < 50000) step(bot.input(game));
  for (int i = 0; i < 30; ++i) step(0);
  shoot("curtain.bmp");
  while (!game.game_over_ready()) step(0);
  for (int i = 0; i < 5; ++i) step(0);
  shoot("gameover.bmp");
  return 0;
}
