#pragma once

#include <cstdint>

#include "core/game.h"

namespace zentris {

// The speech bubble over the pagoda holds 3 lines of 10 characters.
constexpr int kSpeechChars = 10;
constexpr int kSpeechLines = 3;

// Breaks `text` at spaces into at most `max_lines` lines of at most kSpeechChars.
// Returns the number of lines, or 0 if the text doesn't fit.
int wrap_text(const char* text, char out[][kSpeechChars + 1], int max_lines);

struct LineSet {
  const char* const* lines;
  int count;
};

// The oracle's lines for an event (oracle_lines.cpp). Empty for events it ignores.
LineSet oracle_lines(GameEvent event);

// Watches the game for events and decides what the oracle says, and for how long.
class OracleSpeech {
 public:
  void tick(const Game& game);
  bool speaking() const { return line_ != nullptr; }
  const char* line() const { return line_; }

 private:
  const char* line_ = nullptr;
  int frames_left_ = 0;
  int priority_ = 0;
  uint32_t seen_events_ = 0;
  uint32_t rng_ = 0x2545F491u;
};

}  // namespace zentris
