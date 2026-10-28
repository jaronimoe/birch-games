// Minimal self-contained tests for the game core. Run via `ctest` or directly.

#include <cstdio>

#include "bot.h"
#include "core/board.h"
#include "core/game.h"
#include "core/oracle.h"
#include "core/pieces.h"

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

bool has_cell(const Shape& s, int x, int y) {
  for (const Cell& c : s)
    if (c.x == x && c.y == y) return true;
  return false;
}

// Starts a game and releases Start so later presses register.
Game started_game() {
  Game g(1);
  g.tick(kStart);
  g.tick(0);
  return g;
}

void test_rotations() {
  const Shape& t_right = piece_cells(PieceType::T, 1);
  CHECK(has_cell(t_right, 1, 0) && has_cell(t_right, 1, 1) && has_cell(t_right, 1, 2) &&
        has_cell(t_right, 2, 1));

  const Shape& i_right = piece_cells(PieceType::I, 1);
  for (int y = 0; y < 4; ++y) CHECK(has_cell(i_right, 2, y));

  const Shape& i_left = piece_cells(PieceType::I, 3);
  for (int y = 0; y < 4; ++y) CHECK(has_cell(i_left, 1, y));

  for (int r = 1; r < 4; ++r) {
    const Shape& o = piece_cells(PieceType::O, r);
    CHECK(has_cell(o, 1, 0) && has_cell(o, 2, 0) && has_cell(o, 1, 1) && has_cell(o, 2, 1));
  }
}

void test_kicks() {
  // SRS 0->R second and third tests are (-1,0) and (-1,+1 up), i.e. y = -1 in screen space.
  const Kick k1 = kick(PieceType::T, 0, 1, 1);
  const Kick k2 = kick(PieceType::T, 0, 1, 2);
  CHECK(k1.dx == -1 && k1.dy == 0);
  CHECK(k2.dx == -1 && k2.dy == -1);
  CHECK(kick_count(PieceType::O) == 1);
}

void test_remove_rows() {
  Board b;
  for (int x = 0; x < kBoardW; ++x) {
    b.set(x, kBoardH - 1, 1);
    b.set(x, kBoardH - 2, 2);
  }
  b.set(0, kBoardH - 3, 3);
  int rows[kMaxClearRows];
  const int n = b.find_full_rows(rows);
  CHECK(n == 2);
  CHECK(rows[0] == kBoardH - 2 && rows[1] == kBoardH - 1);
  b.remove_rows(rows, n);
  CHECK(b.at(0, kBoardH - 1) == 3);
  for (int x = 1; x < kBoardW; ++x) CHECK(b.at(x, kBoardH - 1) == 0);
  CHECK(b.at(0, kBoardH - 3) == 0);
}

// Fills rows from the bottom; each string is one row, '#' = block.
Board board_from(const char* const* rows, int count) {
  Board b;
  for (int i = 0; i < count; ++i) {
    const int y = kBoardH - count + i;
    for (int x = 0; x < kBoardW; ++x)
      if (rows[i][x] == '#') b.set(x, y, 1);
  }
  return b;
}

void test_oracle_deep_well_gets_i() {
  const char* rows[] = {"######.###", "######.###", "######.###", "######.###"};
  const Board b = board_from(rows, 4);
  for (uint32_t r = 0; r < 16; ++r) {
    const OracleChoice c = choose_piece(b, r);
    CHECK(c.type == PieceType::I);
    CHECK(c.target.lines == 4);
    for (const Cell& cell : piece_cells(c.type, c.target.rot)) CHECK(c.target.x + cell.x == 6);
  }
}

void test_oracle_picks_a_clearing_piece() {
  // The board from the first play test: only the bottom row can be completed.
  const char* rows[] = {".#..#...##", "##..##.###", "######.###"};
  const Board b = board_from(rows, 3);
  CHECK(best_placement(b, PieceType::I).lines == 1);
  CHECK(best_placement(b, PieceType::O).lines == 0);
  for (uint32_t r = 0; r < 32; ++r) {
    const OracleChoice c = choose_piece(b, r);
    CHECK(c.target.valid());
    CHECK(c.target.lines == 1);
  }
}

void test_oracle_fallback_varies() {
  const Board empty;
  bool seen[kPieceCount] = {};
  int distinct = 0;
  for (uint32_t r = 0; r < 64; ++r) {
    const OracleChoice c = choose_piece(empty, r);
    CHECK(c.target.valid());
    CHECK(c.target.lines == 0);
    if (!seen[static_cast<int>(c.type)]) ++distinct;
    seen[static_cast<int>(c.type)] = true;
  }
  CHECK(distinct >= 2);
}

void test_spawn_and_gravity() {
  Game g = started_game();
  CHECK(g.state() == GameState::Playing);
  CHECK(g.has_piece());
  CHECK(g.piece().x == 3 && g.piece().y == kHiddenRows);

  const int y0 = g.piece().y;
  int ticks = 1;  // the release tick in started_game() already counted one gravity frame
  while (g.piece().y == y0 && ticks < 100) {
    g.tick(0);
    ++ticks;
  }
  CHECK(ticks == 48);  // level 0 gravity
}

void test_hard_drop() {
  Game g = started_game();
  const ActivePiece p = g.piece();
  int dist = 0;
  while (g.board().fits(p.type, p.rot, p.x, p.y + dist + 1)) ++dist;

  g.tick(kHardDrop);
  CHECK(g.score() == static_cast<uint32_t>(2 * dist));
  CHECK(!g.has_piece());
  CHECK(g.phase() == Phase::Entry);
  for (int i = 0; i < 6; ++i) g.tick(0);
  CHECK(g.has_piece());
}

void test_das() {
  Game g = started_game();
  const int x0 = g.piece().x;
  g.tick(kRight);
  CHECK(g.piece().x == x0 + 1);  // immediate move on press
  for (int i = 0; i < 9; ++i) g.tick(kRight);
  CHECK(g.piece().x == x0 + 1);  // still charging
  g.tick(kRight);
  CHECK(g.piece().x == x0 + 2);  // auto-repeat kicks in
  for (int i = 0; i < 3; ++i) g.tick(kRight);
  CHECK(g.piece().x == x0 + 3);
  for (int i = 0; i < 30; ++i) g.tick(kRight);
  const ActivePiece& p = g.piece();
  CHECK(!g.board().fits(p.type, p.rot, p.x + 1, p.y));  // stopped at the wall
}

void test_pause() {
  Game g = started_game();
  g.tick(kPause);
  CHECK(g.state() == GameState::Paused);
  const int y = g.piece().y;
  for (int i = 0; i < 200; ++i) g.tick(0);
  CHECK(g.piece().y == y);
  g.tick(kPause);
  CHECK(g.state() == GameState::Playing);
}

void test_reckless_top_out() {
  Game g(7);
  tools::GreedyBot bot;
  bot.set_reckless(true);
  int frames = 0;
  while (g.state() != GameState::GameOver && frames < 5000) {
    g.tick(bot.input(g));
    ++frames;
  }
  CHECK(g.state() == GameState::GameOver);
  CHECK(g.score() > 0);
  CHECK(g.hi_score() == g.score());
  CHECK(g.new_hi_score());
}

void test_bot_clears_lines() {
  Game g(42);
  tools::GreedyBot bot;
  int prev_lines = 0;
  uint32_t prev_score = 0;
  for (int i = 0; i < 20000 && g.state() != GameState::GameOver; ++i) {
    g.tick(bot.input(g));
    if (g.lines() > prev_lines) {
      CHECK(g.score() > prev_score);
      CHECK(g.level() == g.lines() / 10);
    }
    prev_lines = g.lines();
    prev_score = g.score();
  }
  CHECK(g.lines() >= 20);
}

}  // namespace

int main() {
  test_rotations();
  test_kicks();
  test_remove_rows();
  test_oracle_deep_well_gets_i();
  test_oracle_picks_a_clearing_piece();
  test_oracle_fallback_varies();
  test_spawn_and_gravity();
  test_hard_drop();
  test_das();
  test_pause();
  test_reckless_top_out();
  test_bot_clears_lines();
  if (g_failures == 0) std::printf("all tests passed\n");
  return g_failures == 0 ? 0 : 1;
}
