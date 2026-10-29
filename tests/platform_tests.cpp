// Tests for the hardware-free helpers the device ports use (src/platform/common). Run via
// `ctest` or directly; prints how much a dirty-band push sends per frame.

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "bot.h"
#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/oracle_speech.h"
#include "core/render.h"
#include "platform/common/dirty_bands.h"
#include "platform/common/pad_input.h"
#include "platform/common/saved_records.h"

using namespace zentris;

namespace {

int g_failures = 0;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
      ++g_failures;                                                    \
    }                                                                  \
  } while (0)

constexpr int kPixels = kScreenW * kScreenH;
uint16_t g_frame[kPixels];
uint16_t g_panel[kPixels];

// Copies the spans into the stand-in panel, as the device's pushes would.
int push(const RowSpan* spans, int count) {
  int rows = 0;
  for (int i = 0; i < count; ++i) {
    std::memcpy(g_panel + spans[i].y * kScreenW, g_frame + spans[i].y * kScreenW,
                sizeof(uint16_t) * spans[i].h * kScreenW);
    rows += spans[i].h;
  }
  return rows;
}

void test_bands_first_diff_is_whole_frame() {
  Framebuffer fb{g_frame, kScreenW, kScreenH};
  fb.fill_rect(0, 0, kScreenW, kScreenH, rgb(10, 20, 30));
  DirtyBands bands;
  RowSpan spans[DirtyBands::kBandCount];
  CHECK(bands.diff(fb, spans) == 1);
  CHECK(spans[0].y == 0 && spans[0].h == kScreenH);

  bands.invalidate();
  CHECK(bands.diff(fb, spans) == 1);
  CHECK(spans[0].h == kScreenH);
}

void test_bands_report_only_changes() {
  Framebuffer fb{g_frame, kScreenW, kScreenH};
  fb.fill_rect(0, 0, kScreenW, kScreenH, 0);
  DirtyBands bands;
  bands.mark_shown(fb);
  RowSpan spans[DirtyBands::kBandCount];

  // Unchanged: only the round-robin band (band 0 first).
  int n = bands.diff(fb, spans);
  CHECK(n == 1 && spans[0].y == 0 && spans[0].h == DirtyBands::kBandRows);

  // Rows 100..117 touch bands 12, 13 and 14: one merged span. The round-robin band is 1.
  fb.fill_rect(50, 100, 10, 18, 0xFFFF);
  n = bands.diff(fb, spans);
  CHECK(n == 2);
  CHECK(spans[0].y == 8 && spans[0].h == 8);
  CHECK(spans[1].y == 96 && spans[1].h == 24);

  // Pushed now: nothing but the next round-robin band.
  n = bands.diff(fb, spans);
  CHECK(n == 1 && spans[0].y == 16);
}

void test_bands_catch_top_bit_flips() {
  // Two pixels whose red top bit flips: a plain xor-multiply hash would cancel these out.
  Framebuffer fb{g_frame, kScreenW, kScreenH};
  fb.fill_rect(0, 0, kScreenW, kScreenH, 0);
  DirtyBands bands;
  bands.mark_shown(fb);
  RowSpan spans[DirtyBands::kBandCount];
  bands.diff(fb, spans);  // move the round-robin band off band 20

  fb.set(1, 160, 0x8000);
  fb.set(3, 161, 0x8000);
  const int n = bands.diff(fb, spans);
  bool found = false;
  for (int i = 0; i < n; ++i) found |= spans[i].y <= 160 && spans[i].y + spans[i].h > 161;
  CHECK(found);
}

// Plays with the bot (clearing lines first, then topping out twice to cover the curtain and
// the title), renders every tick, pushes only the dirty spans to a stand-in panel and checks
// the panel always matches the frame. Reports what that costs on the wire.
void test_bands_keep_panel_exact_through_games() {
  Framebuffer fb{g_frame, kScreenW, kScreenH};
  Game game(7);
  MysteryNextWindow next;
  OracleSpeech speech;
  tools::GreedyBot bot;
  DirtyBands bands;
  RowSpan spans[DirtyBands::kBandCount];

  render(game, next, speech, fb);
  std::memcpy(g_panel, g_frame, sizeof(g_frame));
  bands.mark_shown(fb);

  long long total_rows = 0;
  int frames = 0, max_rows = 0, over_budget = 0, mismatches = 0, games = 0;
  GameState prev = game.state();
  for (int i = 0; i < 30000 && games < 2; ++i) {
    bot.set_reckless(i >= 10000);
    game.tick(bot.input(game));
    next.tick();
    speech.tick(game);
    if (prev != GameState::GameOver && game.state() == GameState::GameOver) ++games;
    prev = game.state();

    render(game, next, speech, fb);
    const int rows = push(spans, bands.diff(fb, spans));
    if (std::memcmp(g_panel, g_frame, sizeof(g_frame)) != 0) ++mismatches;
    total_rows += rows;
    max_rows = std::max(max_rows, rows);
    // 37.5 MHz SPI, 16 bits a pixel: a 60 Hz frame has room for ~73 rows.
    if (rows * kScreenW * 16 / 37.5e6 > 1.0 / 60) ++over_budget;
    ++frames;
  }
  CHECK(games == 2);
  CHECK(mismatches == 0);

  const double avg_rows = static_cast<double>(total_rows) / frames;
  const double avg_ms = avg_rows * kScreenW * 16 / 37.5e6 * 1000;
  std::printf("dirty bands: %d frames, avg %.1f rows (%.1f KB, %.2f ms at 37.5 MHz), max %d rows,"
              " %.1f%% of frames over a 60 Hz push budget\n",
              frames, avg_rows, avg_rows * kScreenW * 2 / 1024, avg_ms, max_rows,
              100.0 * over_budget / frames);
  CHECK(avg_ms < 8.0);  // well under the 16.7 ms frame
}

void test_debouncer_hides_bounce() {
  ButtonDebouncer d;
  const uint8_t red = pad_bit(kPadRed);
  uint64_t t = 1000000;
  CHECK(d.update(0, t) == 0);
  CHECK(d.update(red, t += 1000) == red);  // a press counts at once
  CHECK(d.update(0, t += 2000) == red);    // bounce, ignored
  CHECK(d.update(red, t += 2000) == red);
  CHECK(d.update(0, t += 2000) == red);
  t += ButtonDebouncer::kLockoutUs;  // released for real
  CHECK(d.update(0, t) == 0);
  CHECK(d.update(red, t += 1000) == 0);  // released bounce, ignored
  CHECK(d.update(red, t += ButtonDebouncer::kLockoutUs) == red);
}

void test_stick_hysteresis_and_dominance() {
  StickInput s;
  CHECK(s.update(0.4f, 0) == 0);
  CHECK(s.update(0.6f, 0) == kRight);
  CHECK(s.update(0.35f, 0) == kRight);  // between release and press: still held
  CHECK(s.update(0.2f, 0) == 0);
  CHECK(s.update(-0.7f, 0) == kLeft);
  CHECK(s.update(0.7f, 0) == kRight);  // flicked straight across

  StickInput d;
  CHECK(d.update(-0.6f, 0.6f) == kLeft);          // diagonal: moves, doesn't drop
  CHECK(d.update(-0.2f, 0.8f) == kDown);          // mostly down
  CHECK(d.update(-0.6f, 0.6f) == (kLeft | kDown));  // once dropping, sideways may join
  CHECK(d.update(0, 0.2f) == 0);
  CHECK(d.update(0, -0.6f) == kUp);
}

void test_pad_mapping() {
  PadInput p;
  p.arm(0);
  PadFrame f = p.update(pad_bit(kPadRed), 0, 0, GameState::Playing);
  CHECK(f.held == kRotateCW && !f.exit);
  f = p.update(pad_bit(kPadYellow), 0, 0, GameState::Playing);
  CHECK(f.held == kRotateCCW);
  f = p.update(pad_bit(kPadGreen), 0, 0, GameState::Playing);
  CHECK(f.held == kRotateCCW);
  f = p.update(pad_bit(kPadBlue), 0, 0, GameState::Playing);
  CHECK(f.held == kHardDrop);
  f = p.update(pad_bit(kPadStickClick), 0, 0, GameState::Playing);
  CHECK(f.held == kHardDrop);
  f = p.update(pad_bit(kPadWhite), 0, 0, GameState::Playing);
  CHECK(f.held == kStart);
  f = p.update(0, 0.9f, 0, GameState::Playing);
  CHECK(f.held == kRight);

  // On the title the diamond's top and bottom pick the mode, like a menu.
  f = p.update(pad_bit(kPadBlue), 0, 0, GameState::Title);
  CHECK(f.held & kUp);
  f = p.update(pad_bit(kPadYellow), 0, 0, GameState::Title);
  CHECK(f.held & kDown);
}

void test_black_pauses_then_exits() {
  PadInput p;
  p.arm(0);
  const uint8_t black = pad_bit(kPadBlack);
  PadFrame f = p.update(black, 0, 0, GameState::Playing);
  CHECK(f.held == kPause && !f.exit);
  f = p.update(black, 0, 0, GameState::Paused);  // still held after pausing: no exit
  CHECK(!f.exit);
  f = p.update(0, 0, 0, GameState::Paused);
  CHECK(!f.exit);
  f = p.update(black, 0, 0, GameState::Paused);
  CHECK(f.exit);

  PadInput t;
  t.arm(0);
  CHECK(t.update(black, 0, 0, GameState::Title).exit);
  PadInput o;
  o.arm(0);
  CHECK(o.update(black, 0, 0, GameState::GameOver).exit);
}

void test_launch_press_is_ignored() {
  // White is still down from the console's menu: no start until released and pressed again.
  PadInput p;
  const uint8_t white = pad_bit(kPadWhite);
  p.arm(white);
  CHECK(p.update(white, 0, 0, GameState::Title).held == 0);
  CHECK(p.update(0, 0, 0, GameState::Title).held == 0);
  CHECK(p.update(white, 0, 0, GameState::Title).held == kStart);

  PadInput b;
  b.arm(pad_bit(kPadBlack));
  CHECK(!b.update(pad_bit(kPadBlack), 0, 0, GameState::Title).exit);
}

// The pad drives a whole game: start from the title, play, game over, back to the title.
void test_pad_drives_game() {
  Game g(3);
  PadInput p;
  p.arm(0);
  auto press = [&](uint8_t buttons) {
    g.tick(p.update(buttons, 0, 0, g.state()).held);
    g.tick(p.update(0, 0, 0, g.state()).held);
  };
  press(pad_bit(kPadWhite));
  CHECK(g.state() == GameState::Playing);
  press(pad_bit(kPadBlack));
  CHECK(g.state() == GameState::Paused);
  press(pad_bit(kPadWhite));
  CHECK(g.state() == GameState::Playing);
  for (int i = 0; i < 20000 && g.state() == GameState::Playing; ++i) press(pad_bit(kPadBlue));
  CHECK(g.state() == GameState::GameOver);
  for (int i = 0; i < 2000 && g.state() == GameState::GameOver; ++i) press(pad_bit(kPadWhite));
  CHECK(g.state() == GameState::Title);
}

SavedRecords g_written;
int g_writes = 0;
void record_write(const SavedRecords& blob) {
  g_written = blob;
  ++g_writes;
}

void test_blob_storage_round_trip() {
  static_assert(sizeof(SavedRecords) == kModeCount * 4 * sizeof(uint32_t), "no padding");
  BlobStorage storage(record_write);
  {
    Game g(11, &storage);
    tools::GreedyBot bot;
    bot.set_reckless(true);
    for (int i = 0; i < 5000 && g.state() != GameState::GameOver; ++i) g.tick(bot.input(g));
    CHECK(g.state() == GameState::GameOver);
    CHECK(g_writes == 1);
    CHECK(g_written.modes[0].score == g.score() && g.score() > 0);
  }

  BlobStorage reloaded(nullptr);
  reloaded.load(g_written);
  Game again(12, &reloaded);
  CHECK(again.records().score == g_written.modes[0].score);
  CHECK(reloaded.load_records(kModeCount).score == 0);  // out of range: nothing

  reloaded.clear();
  Game fresh(13, &reloaded);
  CHECK(fresh.records().score == 0);
}

}  // namespace

int main() {
  test_bands_first_diff_is_whole_frame();
  test_bands_report_only_changes();
  test_bands_catch_top_bit_flips();
  test_bands_keep_panel_exact_through_games();
  test_debouncer_hides_bounce();
  test_stick_hysteresis_and_dominance();
  test_pad_mapping();
  test_black_pauses_then_exits();
  test_launch_press_is_ignored();
  test_pad_drives_game();
  test_blob_storage_round_trip();
  if (g_failures == 0) std::printf("all tests passed\n");
  return g_failures == 0 ? 0 : 1;
}
