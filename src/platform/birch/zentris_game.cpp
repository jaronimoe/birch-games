// The game core behind one birch console screen. Every pass of birch's main loop runs
// update(): it reads the buttons and the stick, advances the game in fixed 60 Hz steps and,
// when it stepped, renders the whole frame into the panel's framebuffer and DMA-pushes only
// the bands that changed (DirtyBands). The framebuffer's host-endian RGB565 goes out as is:
// the DMA push sends 16-bit SPI frames, MSB first, which is what the ST7789 wants.

#include "platform/birch/zentris_game.h"

#include <cstdio>

#include "pico/rand.h"
#include "pico/time.h"

#include "console.h"
#include "hardware.h"
#include "menu_input.h"
#include "pigs.h"
#include "storage.h"

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/oracle_speech.h"
#include "core/render.h"
#include "platform/common/dirty_bands.h"
#include "platform/common/pad_input.h"
#include "platform/common/saved_records.h"

namespace {

static_assert(zentris::kScreenW == kScreenW && zentris::kScreenH == kScreenH,
              "Zentris draws the whole panel");

constexpr uint64_t kStepUs = 1000000 / zentris::kTicksPerSecond;
// After a stall (a records save takes ~50 ms) the game catches up this many steps, no more.
constexpr uint64_t kMaxBehindUs = 5 * kStepUs;
constexpr uint64_t kStatsEveryUs = 10 * 1000000;  // how often the USB log gets a stats line

struct ButtonMap {
  HardwareButton hw;
  zentris::PadButton pad;
};
// birch's PINOUT.md: red 1 (right of the diamond), green 2 (left), blue 3 (top), yellow 4
// (bottom), black 5 (left of the screen), white 6 (right of it).
constexpr ButtonMap kButtons[] = {
    {HW_BUTTON_1, zentris::kPadRed},   {HW_BUTTON_2, zentris::kPadGreen},
    {HW_BUTTON_3, zentris::kPadBlue},  {HW_BUTTON_4, zentris::kPadYellow},
    {HW_BUTTON_5, zentris::kPadBlack}, {HW_BUTTON_6, zentris::kPadWhite},
};

void write_records(const zentris::SavedRecords& blob) {
  storage_put(kStorageZentrisRecords, &blob, sizeof(blob));
}

zentris::BlobStorage s_storage(write_records);
zentris::Game s_game(1);
zentris::MysteryNextWindow s_next;
zentris::OracleSpeech s_speech;
zentris::ButtonDebouncer s_debouncer;
zentris::PadInput s_pad;
zentris::DirtyBands s_bands;
zentris::RowSpan s_spans[zentris::DirtyBands::kBandCount];

uint64_t s_last_us = 0;
uint64_t s_behind_us = 0;  // time not stepped yet
int s_saved_clock = 0;     // SETTINGS' SPI clock and stick curve, put back on leaving
float s_saved_curve = 1.0f;

uint64_t s_stats_since_us = 0;
uint32_t s_frames = 0;
uint64_t s_render_us = 0;
uint64_t s_rows = 0;

zentris::Framebuffer framebuffer() {
  return {hardware_display_framebuffer(), kScreenW, kScreenH};
}

// Buttons down now (dev_serial's held ones included), plus any press since the last pass,
// so a tap between two passes still lands. Reading the presses also consumes them, so none
// lingers for the console's menu.
uint8_t read_buttons() {
  uint8_t raw = 0;
  for (const ButtonMap& b : kButtons) {
    const bool pressed = hardware_button_pressed(b.hw);
    if (pressed || hardware_button_down(b.hw)) raw |= zentris::pad_bit(b.pad);
  }
  joystick_t* stick = hardware_get_joystick();
  const bool pressed = joystick_button_pressed(stick);
  if (pressed || joystick_button_held(stick)) raw |= zentris::pad_bit(zentris::kPadStickClick);
  return raw;
}

void read_stick(float* x, float* y) {
  if (!hardware_dev_stick(x, y)) joystick_read(hardware_get_joystick(), x, y);
}

void draw_frame(zentris::Framebuffer& fb) {
  zentris::render(s_game, s_next, s_speech, fb);
  // The console's battery icon, in the sky's top-right corner. Nothing without a cell.
  ui_draw_battery_icon(kScreenW - kUiBatteryIconW - 4, kUiBatteryIndicatorTopPx, false);
}

void present() {
  hardware_display_push_wait();  // the last push reads the framebuffer about to be drawn into
  const uint64_t start = time_us_64();
  zentris::Framebuffer fb = framebuffer();
  draw_frame(fb);
  const int count = s_bands.diff(fb, s_spans);
  s_render_us += time_us_64() - start;
  // Each push waits for the one before; the last runs on while the game steps.
  for (int i = 0; i < count; ++i) {
    const zentris::RowSpan& span = s_spans[i];
    hardware_display_push_async(0, span.y, kScreenW, span.h, fb.px + span.y * kScreenW);
    s_rows += span.h;
  }
  ++s_frames;
}

void reset_stats(uint64_t now) {
  s_stats_since_us = now;
  s_frames = 0;
  s_render_us = 0;
  s_rows = 0;
}

// For checking the frame rate on the device: frames a second, the time to render and find
// the changed bands, and the rows pushed a frame (at 37.5 MHz a row takes ~0.14 ms).
void log_stats(uint64_t now) {
  if (now - s_stats_since_us < kStatsEveryUs || s_frames == 0) return;
  const float seconds = (now - s_stats_since_us) / 1e6f;
  printf("zentris: %.1f fps, render %.2f ms, %.1f rows pushed a frame\n", s_frames / seconds,
         s_render_us / 1000.0f / s_frames, static_cast<float>(s_rows) / s_frames);
  reset_stats(now);
}

void leave() {
  hardware_display_push_wait();
  hardware_set_display_clock(s_saved_clock);
  joystick_set_curve_exponent(hardware_get_joystick(), s_saved_curve);
  console_exit_game();
}

// The console hid to black before launching: draw the title and reveal it.
void on_enter() {
  pigs_set_rotation(PIGS_ROTATION_0);
  menu_input_clear_touch();
  joystick_t* stick = hardware_get_joystick();
  s_saved_curve = joystick_get_curve_exponent(stick);
  joystick_set_curve_exponent(stick, 1.0f);  // StickInput's thresholds are linear travel
  joystick_suppress_until_centered(stick, zentris::StickInput::kRelease);
  // The fastest SPI clock whatever SETTINGS says, as FLIGHT does.
  s_saved_clock = hardware_get_display_clock();
  hardware_set_display_clock(hardware_display_clock_count() - 1);

  s_game = zentris::Game(get_rand_32(), &s_storage);
  s_next = zentris::MysteryNextWindow();
  s_speech = zentris::OracleSpeech();
  s_debouncer = zentris::ButtonDebouncer();
  s_pad = zentris::PadInput();
  s_pad.arm(s_debouncer.update(read_buttons(), time_us_64()));

  hardware_display_push_wait();
  zentris::Framebuffer fb = framebuffer();
  draw_frame(fb);
  ui_reveal_from_black();
  s_bands.mark_shown(fb);

  s_last_us = time_us_64();
  s_behind_us = 0;
  reset_stats(s_last_us);
}

void update() {
  menu_input_clear_touch();  // touch isn't a control yet (roadmap 5); none may linger
  const uint64_t now = time_us_64();
  float x, y;
  read_stick(&x, &y);
  const uint8_t buttons = s_debouncer.update(read_buttons(), now);
  const zentris::PadFrame pad = s_pad.update(buttons, x, y, s_game.state());
  if (pad.exit) {
    leave();
    return;
  }

  s_behind_us += now - s_last_us;
  s_last_us = now;
  if (s_behind_us > kMaxBehindUs) s_behind_us = kMaxBehindUs;
  if (s_behind_us < kStepUs) return;
  while (s_behind_us >= kStepUs) {
    s_game.tick(pad.held);
    s_next.tick();
    s_speech.tick(s_game);
    s_behind_us -= kStepUs;
  }
  present();
  log_stats(now);
}

const char* get_name() { return "Zentris"; }

const UiScreen kScreen = {get_name, on_enter, update};

}  // namespace

const UiScreen* zentris_game_init() {
  zentris::SavedRecords saved;
  if (storage_get(kStorageZentrisRecords, &saved, sizeof(saved))) s_storage.load(saved);
  return &kScreen;
}

void zentris_game_factory_reset() { s_storage.clear(); }
