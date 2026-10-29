#pragma once

#include <cstdint>

#include "core/game.h"

namespace zentris {

// The handheld's buttons as a port reads them: the colored diamond (blue top, yellow bottom,
// green left, red right), white right of the screen, black left of it, and the stick's
// click. A mask of them has bit n set for PadButton n.
enum PadButton : uint8_t {
  kPadRed,
  kPadYellow,
  kPadGreen,
  kPadBlue,
  kPadWhite,
  kPadBlack,
  kPadStickClick,
  kPadButtonCount,
};

constexpr uint8_t pad_bit(PadButton b) { return static_cast<uint8_t>(1u << b); }

// Hides contact bounce: a change of a button's raw level counts at once, then that button's
// pin is ignored for kLockoutUs. Without it a bouncing press could pause and unpause.
class ButtonDebouncer {
 public:
  static constexpr uint64_t kLockoutUs = 30000;

  // `raw`: buttons physically down now. Returns the debounced mask.
  uint8_t update(uint8_t raw, uint64_t now_us);

 private:
  uint8_t state_ = 0;
  uint64_t ready_us_[kPadButtonCount] = {};  // when each button may change again
};

// Turns the analog stick into held directions, with hysteresis so a stick resting near the
// threshold doesn't flicker. Sideways always counts; down (soft drop) and up only when the
// stick points mostly that way, so a diagonal push moves without dropping.
class StickInput {
 public:
  // Untuned first guesses, in joystick_read() units (-1..1).
  static constexpr float kPress = 0.5f;
  static constexpr float kRelease = 0.3f;
  static constexpr float kDominance = 2.0f;  // down/up start only when |y| >= this * |x|

  // y > 0 is down. Returns kLeft/kRight/kUp/kDown bits.
  uint16_t update(float x, float y);

 private:
  uint16_t held_ = 0;
};

// What PadInput asks of the port for one pass.
struct PadFrame {
  uint16_t held = 0;  // Actions to hold for the game's next ticks
  bool exit = false;  // leave the game
};

// Zentris's controls on the handheld:
//   stick           move, soft drop; up/down pick the mode on the title
//   red             rotate clockwise
//   yellow, green   rotate counter-clockwise
//   blue, click     hard drop (blue/yellow pick the mode on the title, like a menu)
//   white           start / pause
//   black           pause while playing; anywhere else, exit
class PadInput {
 public:
  // Call on entering the game, with the buttons down at that moment: each counts only once
  // it has been released, so the press that launched the game doesn't also start it.
  void arm(uint8_t buttons_down);

  // `buttons`: debounced PadButton mask. `x`, `y`: the stick (-1..1, y > 0 down). `state`:
  // the game's state as of now.
  PadFrame update(uint8_t buttons, float x, float y, GameState state);

 private:
  StickInput stick_;
  uint8_t ignored_ = 0;
  uint8_t last_ = 0;
};

}  // namespace zentris
