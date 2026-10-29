#include "platform/common/pad_input.h"

#include <cmath>

namespace zentris {

uint8_t ButtonDebouncer::update(uint8_t raw, uint64_t now_us) {
  for (int b = 0; b < kPadButtonCount; ++b) {
    const uint8_t bit = static_cast<uint8_t>(1u << b);
    if ((raw & bit) == (state_ & bit) || now_us < ready_us_[b]) continue;
    state_ ^= bit;
    ready_us_[b] = now_us + kLockoutUs;
  }
  return state_;
}

uint16_t StickInput::update(float x, float y) {
  uint16_t next = 0;

  if ((held_ & kLeft) && x <= -kRelease) next |= kLeft;
  if ((held_ & kRight) && x >= kRelease) next |= kRight;
  if (!(next & (kLeft | kRight))) {
    if (x <= -kPress) next |= kLeft;
    if (x >= kPress) next |= kRight;
  }

  const float side = kDominance * std::fabs(x);
  if ((held_ & kDown) && y >= kRelease) next |= kDown;
  if ((held_ & kUp) && y <= -kRelease) next |= kUp;
  if (!(next & (kUp | kDown))) {
    if (y >= kPress && y >= side) next |= kDown;
    if (-y >= kPress && -y >= side) next |= kUp;
  }

  held_ = next;
  return next;
}

void PadInput::arm(uint8_t buttons_down) {
  ignored_ = buttons_down;
  last_ = buttons_down;
}

PadFrame PadInput::update(uint8_t buttons, float x, float y, GameState state) {
  ignored_ &= buttons;  // released buttons count again
  const uint8_t down = buttons & ~ignored_;
  const uint8_t pressed = down & ~last_;
  last_ = down;

  PadFrame f;
  f.held = stick_.update(x, y);
  const bool playing = state == GameState::Playing;
  if (!playing && (pressed & pad_bit(kPadBlack))) f.exit = true;

  if (down & pad_bit(kPadRed)) f.held |= kRotateCW;
  if (down & (pad_bit(kPadYellow) | pad_bit(kPadGreen))) f.held |= kRotateCCW;
  if (down & (pad_bit(kPadBlue) | pad_bit(kPadStickClick))) f.held |= kHardDrop;
  if (down & pad_bit(kPadWhite)) f.held |= kStart;
  if (playing && (down & pad_bit(kPadBlack))) f.held |= kPause;
  if (state == GameState::Title) {
    if (down & pad_bit(kPadBlue)) f.held |= kUp;
    if (down & pad_bit(kPadYellow)) f.held |= kDown;
  }
  return f;
}

}  // namespace zentris
