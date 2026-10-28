#include "core/game.h"

namespace zentris {
namespace {

constexpr int kDasDelay = 10;  // frames a direction is held before it auto-repeats
constexpr int kDasRepeat = 3;  // frames between auto-repeat moves
constexpr int kSoftDropFrames = 2;
constexpr int kLockDelay = 30;
constexpr int kMaxLockResets = 15;
constexpr int kEntryDelay = 6;
constexpr int kClearStepFrames = 4;
constexpr int kLineClearFrames = kClearStepFrames * 5;
constexpr int kCurtainRowFrames = 3;
constexpr int kCurtainFrames = kCurtainRowFrames * kVisibleRows;
constexpr int kGameOverInputDelay = kCurtainFrames + 30;
constexpr int kSpawnX = 3;

constexpr uint32_t kLineScores[kMaxClearRows + 1] = {0, 40, 100, 300, 1200};

// Frames per row of gravity, NES style.
int gravity_frames(int level) {
  static constexpr uint8_t kTable[] = {48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
                                       5,  5,  5,  4,  4,  4,  3,  3,  3};
  if (level < static_cast<int>(sizeof(kTable))) return kTable[level];
  return level < 29 ? 2 : 1;
}

}  // namespace

Game::Game(uint32_t seed) : rng_(seed ? seed : 0x9E3779B9u) {}

void Game::tick(uint16_t held) {
  const uint16_t pressed = held & ~prev_held_;
  prev_held_ = held;
  ++frame_;

  switch (state_) {
    case GameState::Title:
      random();  // the moment Start is pressed seeds the run
      if (pressed & kStart) start();
      break;
    case GameState::Paused:
      if (pressed & (kStart | kPause)) state_ = GameState::Playing;
      break;
    case GameState::GameOver:
      ++phase_timer_;
      if (game_over_ready() && (pressed & kStart)) state_ = GameState::Title;
      break;
    case GameState::Playing:
      if (pressed & (kStart | kPause)) {
        state_ = GameState::Paused;
        break;
      }
      tick_playing(held, pressed);
      break;
  }
}

bool Game::is_clearing_row(int y) const {
  if (phase_ != Phase::LineClear) return false;
  for (int i = 0; i < clear_count_; ++i)
    if (clear_rows_[i] == y) return true;
  return false;
}

int Game::line_clear_step() const {
  const int step = phase_timer_ / kClearStepFrames;
  return step > 4 ? 4 : step;
}

int Game::curtain_rows() const {
  const int rows = phase_timer_ / kCurtainRowFrames;
  return rows > kVisibleRows ? kVisibleRows : rows;
}

bool Game::game_over_ready() const {
  return state_ == GameState::GameOver && phase_timer_ >= kGameOverInputDelay;
}

void Game::start() {
  board_.clear();
  score_ = 0;
  lines_ = 0;
  level_ = 0;
  new_hi_ = false;
  clear_count_ = 0;
  das_dir_ = 0;
  das_timer_ = 0;
  bag_pos_ = kPieceCount;
  state_ = GameState::Playing;
  spawn();
}

void Game::spawn() {
  piece_ = {next_piece(), 0, kSpawnX, kHiddenRows};
  phase_ = Phase::Falling;
  phase_timer_ = 0;
  gravity_timer_ = 0;
  lock_timer_ = 0;
  lock_resets_ = 0;
  lowest_y_ = piece_.y;
  if (!fits(0, 0)) {
    game_over();
    return;
  }
  has_piece_ = true;
}

void Game::lock_piece() {
  board_.place(piece_.type, piece_.rot, piece_.x, piece_.y);
  has_piece_ = false;

  bool all_hidden = true;
  for (const Cell& c : piece_cells(piece_.type, piece_.rot))
    if (piece_.y + c.y >= kHiddenRows) all_hidden = false;
  if (all_hidden) {
    game_over();
    return;
  }

  phase_timer_ = 0;
  clear_count_ = board_.find_full_rows(clear_rows_);
  if (clear_count_ == 0) {
    phase_ = Phase::Entry;
    return;
  }
  score_ += kLineScores[clear_count_] * static_cast<uint32_t>(level_ + 1);
  lines_ += clear_count_;
  level_ = lines_ / 10;
  phase_ = Phase::LineClear;
}

void Game::game_over() {
  has_piece_ = false;
  state_ = GameState::GameOver;
  phase_timer_ = 0;
  if (score_ > hi_score_) {
    hi_score_ = score_;
    new_hi_ = true;
  }
}

void Game::tick_playing(uint16_t held, uint16_t pressed) {
  switch (phase_) {
    case Phase::Falling:
      tick_falling(held, pressed);
      break;
    case Phase::Entry:
      das_step(held);  // keep the auto-repeat charged between pieces
      if (++phase_timer_ >= kEntryDelay) spawn();
      break;
    case Phase::LineClear:
      das_step(held);
      if (++phase_timer_ >= kLineClearFrames) {
        board_.remove_rows(clear_rows_, clear_count_);
        clear_count_ = 0;
        spawn();
      }
      break;
  }
}

void Game::tick_falling(uint16_t held, uint16_t pressed) {
  if (const int dir = das_step(held))
    if (try_move(dir, 0)) after_move();
  if (pressed & kRotateCW) try_rotate(1);
  if (pressed & kRotateCCW) try_rotate(-1);

  if (pressed & kHardDrop) {
    uint32_t dist = 0;
    while (try_move(0, 1)) ++dist;
    score_ += 2 * dist;
    lock_piece();
    return;
  }

  const bool soft = held & kDown;
  int interval = gravity_frames(level_);
  if (soft && interval > kSoftDropFrames) interval = kSoftDropFrames;
  if (++gravity_timer_ >= interval) {
    gravity_timer_ = 0;
    if (try_move(0, 1)) {
      if (soft) score_ += 1;
      if (piece_.y > lowest_y_) {
        lowest_y_ = piece_.y;
        lock_resets_ = 0;
      }
    }
  }

  if (fits(0, 1)) {
    lock_timer_ = 0;
    return;
  }
  if (++lock_timer_ >= kLockDelay) lock_piece();
}

// Returns -1/+1 on frames where the held direction should move the piece, else 0.
int Game::das_step(uint16_t held) {
  int dir = 0;
  if (held & kLeft) dir -= 1;
  if (held & kRight) dir += 1;
  if (dir == 0) {
    das_dir_ = 0;
    das_timer_ = 0;
    return 0;
  }
  if (dir != das_dir_) {
    das_dir_ = dir;
    das_timer_ = 0;
    return dir;
  }
  if (++das_timer_ >= kDasDelay) {
    das_timer_ = kDasDelay - kDasRepeat;
    return dir;
  }
  return 0;
}

bool Game::fits(int dx, int dy) const {
  return board_.fits(piece_.type, piece_.rot, piece_.x + dx, piece_.y + dy);
}

bool Game::try_move(int dx, int dy) {
  if (!fits(dx, dy)) return false;
  piece_.x += dx;
  piece_.y += dy;
  return true;
}

void Game::try_rotate(int dir) {
  const int rot = (piece_.rot + dir) & 3;
  for (int i = 0; i < kick_count(piece_.type); ++i) {
    const Kick k = kick(piece_.type, piece_.rot, dir, i);
    if (board_.fits(piece_.type, rot, piece_.x + k.dx, piece_.y + k.dy)) {
      piece_.rot = rot;
      piece_.x += k.dx;
      piece_.y += k.dy;
      after_move();
      return;
    }
  }
}

// A successful move or rotation on the ground buys more time, up to a limit that only
// resets once the piece reaches a new lowest row.
void Game::after_move() {
  if (piece_.y > lowest_y_) {
    lowest_y_ = piece_.y;
    lock_resets_ = 0;
  }
  if (lock_timer_ > 0 && lock_resets_ < kMaxLockResets) {
    lock_timer_ = 0;
    ++lock_resets_;
  }
}

// 7-bag randomizer. Milestone 2 replaces this with the oracle.
PieceType Game::next_piece() {
  if (bag_pos_ >= kPieceCount) {
    for (int i = 0; i < kPieceCount; ++i) bag_[i] = static_cast<PieceType>(i);
    for (int i = kPieceCount - 1; i > 0; --i) {
      const int j = static_cast<int>(random() % static_cast<uint32_t>(i + 1));
      const PieceType t = bag_[i];
      bag_[i] = bag_[j];
      bag_[j] = t;
    }
    bag_pos_ = 0;
  }
  return bag_[bag_pos_++];
}

uint32_t Game::random() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return rng_;
}

}  // namespace zentris
