// Everything the oracle says. Edit freely: uppercase letters, digits and . , ! ? : ' - /
// Each line must wrap into 3 lines of at most 10 characters (the tests check this).

#include "core/oracle_speech.h"

namespace zentris {
namespace {

constexpr const char* kStartLines[] = {
    "BREATHE IN.",
    "THE STACK AWAITS.",
    "I WILL PROVIDE.",
    "TRUST THE PIECES.",
};

constexpr const char* kAsForeseenLines[] = {
    "AS FORESEEN.",
    "JUST AS I PLANNED.",
    "YOU SEE IT TOO.",
    "OBVIOUSLY.",
    "MMHM.",
    "WE ARE ONE.",
};

constexpr const char* kBoldLines[] = {
    "BOLD.",
    "NOT MY PLAN. BETTER.",
    "YOU DEFY FATE.",
    "HM. CURIOUS.",
};

constexpr const char* kStreakBrokenLines[] = {
    "THAT HOLE WAS NOT IN THE PLAN.",
    "A HOLE. HOW HUMAN.",
    "EVEN I DID NOT SEE THAT.",
    "BREATHE OUT.",
};

constexpr const char* kLevelUpLines[] = {
    "FASTER NOW.",
    "THE RIVER QUICKENS.",
    "DEEPER.",
    "LEVEL UP. I KNEW IT.",
};

constexpr const char* kFourLineClearLines[] = {
    "I SAW THIS COMING.",
    "FOUR. AS THE STARS SAID.",
    "BEAUTIFUL.",
    "IT IS WRITTEN.",
};

constexpr const char* kGameOverLines[] = {
    "AS ALWAYS, IT ENDS.",
    "AGAIN, THEN?",
    "THE STACK RETURNS TO DUST.",
    "REST NOW.",
};

template <int N>
constexpr LineSet set_of(const char* const (&lines)[N]) {
  return {lines, N};
}

}  // namespace

LineSet oracle_lines(GameEvent event) {
  switch (event) {
    case GameEvent::Start: return set_of(kStartLines);
    case GameEvent::AsForeseen: return set_of(kAsForeseenLines);
    case GameEvent::Bold: return set_of(kBoldLines);
    case GameEvent::StreakBroken: return set_of(kStreakBrokenLines);
    case GameEvent::LevelUp: return set_of(kLevelUpLines);
    case GameEvent::FourLines: return set_of(kFourLineClearLines);
    case GameEvent::GameOver: return set_of(kGameOverLines);
    case GameEvent::None: break;
  }
  return {nullptr, 0};
}

}  // namespace zentris
