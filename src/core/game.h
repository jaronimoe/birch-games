#pragma once

#include <cstdint>

#include "core/board.h"
#include "core/oracle.h"
#include "core/pieces.h"
#include "core/storage.h"

namespace zentris {

// Logical inputs. The platform layer maps stick, buttons and keys onto these.
enum Action : uint16_t {
  kLeft = 1 << 0,
  kRight = 1 << 1,
  kUp = 1 << 2,
  kDown = 1 << 3,
  kHardDrop = 1 << 4,
  kRotateCW = 1 << 5,
  kRotateCCW = 1 << 6,
  kStart = 1 << 7,
  kPause = 1 << 8,
};

enum class GameState : uint8_t { Title, Playing, Paused, GameOver };

// Sub-state while playing.
enum class Phase : uint8_t {
  Falling,    // a piece is under player control
  Entry,      // short delay before the next piece appears
  LineClear,  // full rows are being wiped
};

// Things the oracle may comment on. At most one per locked piece.
enum class GameEvent : uint8_t {
  None,
  Start,
  AsForeseen,    // cleared lines exactly where the oracle meant the piece to go
  Bold,          // cleared more lines than the oracle planned (tucks, spins)
  StreakBroken,  // created a hole while the streak had a multiplier
  LevelUp,
  FourLines,
  GameOver,
};

struct ActivePiece {
  PieceType type;
  int rot, x, y;
};

constexpr int kTicksPerSecond = 60;

// Line-clear multiplier for a clean streak (pieces placed in a row without a new hole).
int streak_multiplier(int streak);

class Game {
 public:
  // `storage` (optional) keeps the high score between sessions.
  explicit Game(uint32_t seed, Storage* storage = nullptr);

  // Advances one fixed 1/60 s step. `held` is the bitmask of Actions held this step.
  void tick(uint16_t held);

  GameState state() const { return state_; }
  Phase phase() const { return phase_; }
  const Board& board() const { return board_; }
  bool has_piece() const { return has_piece_; }
  const ActivePiece& piece() const { return piece_; }
  // Where the oracle meant the current piece to go. For debugging; the player never sees it.
  const Placement& oracle_target() const { return oracle_target_; }
  uint32_t frame() const { return frame_; }

  uint32_t score() const { return score_; }
  uint32_t hi_score() const { return hi_score_ > score_ ? hi_score_ : score_; }
  bool new_hi_score() const { return new_hi_; }
  int level() const { return level_; }
  int lines() const { return lines_; }
  int streak() const { return streak_; }
  int best_streak() const { return best_streak_; }
  int multiplier() const { return streak_multiplier(streak_); }

  // The most recent event; event_count() goes up by one per event so observers can spot new ones.
  GameEvent last_event() const { return last_event_; }
  uint32_t event_count() const { return event_count_; }

  // Replaces the board contents. For tests and debug tools.
  void load_board(const Board& board) { board_ = board; }

  // Line clear animation: rows being wiped and how far the wipe has spread (0..4).
  bool is_clearing_row(int y) const;
  int clear_count() const { return clear_count_; }
  int line_clear_step() const;

  // Game over animation: visible rows covered from the top, and whether it finished.
  int curtain_rows() const;
  bool game_over_ready() const;

 private:
  void start();
  void spawn();
  void lock_piece();
  void game_over();
  void tick_playing(uint16_t held, uint16_t pressed);
  void tick_falling(uint16_t held, uint16_t pressed);
  int das_step(uint16_t held);
  bool fits(int dx, int dy) const;
  bool try_move(int dx, int dy);
  void try_rotate(int dir);
  void after_move();
  void emit(GameEvent event);
  uint32_t random();

  Storage* storage_;
  Board board_;
  GameState state_ = GameState::Title;
  Phase phase_ = Phase::Entry;
  ActivePiece piece_{PieceType::I, 0, 0, 0};
  Placement oracle_target_;
  bool has_piece_ = false;

  uint32_t rng_;

  uint32_t frame_ = 0;
  uint16_t prev_held_ = 0;
  int phase_timer_ = 0;
  int gravity_timer_ = 0;
  int lock_timer_ = 0;
  int lock_resets_ = 0;
  int lowest_y_ = 0;
  int das_dir_ = 0;
  int das_timer_ = 0;

  int clear_rows_[kMaxClearRows] = {};
  int clear_count_ = 0;

  uint32_t score_ = 0;
  uint32_t hi_score_ = 0;
  bool new_hi_ = false;
  int level_ = 0;
  int lines_ = 0;
  int streak_ = 0;
  int best_streak_ = 0;

  GameEvent last_event_ = GameEvent::None;
  uint32_t event_count_ = 0;
};

}  // namespace zentris
