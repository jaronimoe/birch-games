#include "core/oracle_speech.h"

#include <cstring>

namespace zentris {
namespace {

constexpr int kSpeechFrames = 150;
constexpr int kGameOverSpeechFrames = 300;

// A new line only interrupts one of equal or lower priority.
int priority(GameEvent event) {
  switch (event) {
    case GameEvent::GameOver: return 5;
    case GameEvent::FourLines: return 4;
    case GameEvent::LevelUp:
    case GameEvent::Bold: return 3;
    case GameEvent::StreakBroken: return 2;
    case GameEvent::Start:
    case GameEvent::AsForeseen: return 1;
    case GameEvent::None: break;
  }
  return 0;
}

}  // namespace

int wrap_text(const char* text, char out[][kSpeechChars + 1], int max_lines) {
  int line = 0;
  int len = 0;
  out[0][0] = '\0';
  for (const char* p = text; *p;) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* word = p;
    while (*p && *p != ' ') ++p;
    const int word_len = static_cast<int>(p - word);
    if (word_len > kSpeechChars) return 0;
    if (len > 0 && len + 1 + word_len > kSpeechChars) {
      if (++line >= max_lines) return 0;
      len = 0;
    }
    if (len > 0) out[line][len++] = ' ';
    std::memcpy(out[line] + len, word, static_cast<size_t>(word_len));
    len += word_len;
    out[line][len] = '\0';
  }
  return len == 0 ? 0 : line + 1;
}

void OracleSpeech::tick(const Game& game) {
  if (frames_left_ > 0 && --frames_left_ == 0) {
    line_ = nullptr;
    priority_ = 0;
  }
  if (game.event_count() == seen_events_) return;
  seen_events_ = game.event_count();

  const GameEvent event = game.last_event();
  const LineSet set = oracle_lines(event);
  if (set.count == 0 || (line_ && priority(event) < priority_)) return;

  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  int pick = static_cast<int>(rng_ % static_cast<uint32_t>(set.count));
  if (set.count > 1 && set.lines[pick] == line_) pick = (pick + 1) % set.count;  // no repeats

  line_ = set.lines[pick];
  priority_ = priority(event);
  frames_left_ = event == GameEvent::GameOver ? kGameOverSpeechFrames : kSpeechFrames;
}

}  // namespace zentris
