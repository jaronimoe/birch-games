// Minimal self-contained tests for the game core. Run via `ctest` or directly.

#include <cstdio>
#include <string>

#include "bot.h"
#include "core/board.h"
#include "core/game.h"
#include "core/oracle.h"
#include "core/oracle_speech.h"
#include "core/pieces.h"
#include "core/storage.h"

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

bool is_one_of(PieceType t, const char* names) {
  const char kNames[] = "IOTSZJL";
  for (; *names; ++names)
    if (kNames[static_cast<int>(t)] == *names) return true;
  return false;
}

// Play-test board: six pieces clear the bottom row, but only T and S do it without
// leaving a hole or a tower.
void test_oracle_prefers_clean_clears() {
  const char* rows[] = {"#.........", "#........#", "##.#######"};
  const Board b = board_from(rows, 3);
  for (uint32_t r = 0; r < 64; ++r) {
    const OracleChoice c = choose_piece(b, r);
    CHECK(is_one_of(c.type, "TS"));
    CHECK(c.target.lines == 1);
    CHECK(c.target.new_holes == 0);
  }
}

// Play-test boards where no piece clears a line: the oracle fills one of the two bottom
// gaps without creating a hole. With tall walls next to the gaps, J and L can't do that.
void test_oracle_fills_lowest_gap() {
  const char* walled[] = {"......##..", "##....##..", "###...###.", "###...###.", "###...###.",
                          "###...###.", "###...###.", "###...###.", "###.#.####"};
  const char* open[] = {".......#..", ".......#..", "#.#...####", "###.#.####"};
  const Board walled_board = board_from(walled, 9);
  const Board open_board = board_from(open, 4);
  bool walled_seen[kPieceCount] = {};
  bool open_seen[kPieceCount] = {};
  for (uint32_t r = 0; r < 64; ++r) {
    const OracleChoice w = choose_piece(walled_board, r);
    CHECK(is_one_of(w.type, "ITSZ"));
    CHECK(w.target.lines == 0 && w.target.new_holes == 0 && w.target.deepest == kBoardH - 1);
    walled_seen[static_cast<int>(w.type)] = true;

    const OracleChoice o = choose_piece(open_board, r);
    CHECK(is_one_of(o.type, "ITSZJL"));
    CHECK(o.target.lines == 0 && o.target.new_holes == 0 && o.target.deepest == kBoardH - 1);
    open_seen[static_cast<int>(o.type)] = true;
  }
  // Every clean gap-filler gets dealt, not just the flattest one.
  for (int t = 0; t < kPieceCount; ++t) {
    CHECK(walled_seen[t] == is_one_of(static_cast<PieceType>(t), "ITSZ"));
    CHECK(open_seen[t] == is_one_of(static_cast<PieceType>(t), "ITSZJL"));
  }
}

// On an empty board, every piece that can lie flat without a hole is fair game; S and Z
// always leave one.
void test_oracle_empty_board() {
  const Board empty;
  bool seen[kPieceCount] = {};
  for (uint32_t r = 0; r < 64; ++r) {
    const OracleChoice c = choose_piece(empty, r);
    CHECK(c.target.valid());
    CHECK(c.target.lines == 0 && c.target.new_holes == 0);
    seen[static_cast<int>(c.type)] = true;
  }
  for (int t = 0; t < kPieceCount; ++t)
    CHECK(seen[t] == is_one_of(static_cast<PieceType>(t), "IOTJL"));
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

class MemoryStorage : public Storage {
 public:
  explicit MemoryStorage(uint32_t hi) : hi_(hi) {}
  uint32_t load_hi_score() override { return hi_; }
  void save_hi_score(uint32_t score) override {
    hi_ = score;
    ++saves_;
  }
  uint32_t hi_ = 0;
  int saves_ = 0;
};

// Hard-drops the first piece, then swaps in `board` before the next piece is chosen.
void start_with_board(Game& g, const Board& board) {
  g.tick(kStart);
  g.tick(0);
  g.tick(kHardDrop);
  g.load_board(board);
  g.tick(0);
}

// Lets the bot play until the game reports a new event; returns it.
GameEvent next_event(Game& g, tools::GreedyBot& bot) {
  const uint32_t count = g.event_count();
  for (int i = 0; i < 2000 && g.event_count() == count; ++i) g.tick(bot.input(g));
  return g.event_count() == count ? GameEvent::None : g.last_event();
}

void test_streak_multiplier() {
  CHECK(streak_multiplier(0) == 1);
  CHECK(streak_multiplier(9) == 1);
  CHECK(streak_multiplier(10) == 2);
  CHECK(streak_multiplier(25) == 3);
  CHECK(streak_multiplier(49) == 3);
  CHECK(streak_multiplier(50) == 4);
  CHECK(streak_multiplier(500) == 4);
}

void test_streak_counts_and_breaks() {
  Game g(5);
  g.tick(kStart);
  g.tick(0);
  g.tick(kHardDrop);  // the oracle only deals flat, hole-free pieces on an empty board
  CHECK(g.streak() == 1);

  // A lone block under the spawn column: any piece dropped straight down bridges it.
  const char* bump[] = {"....#....."};
  g.load_board(board_from(bump, 1));
  while (!g.has_piece()) g.tick(0);
  g.tick(kHardDrop);
  CHECK(g.streak() == 0);
  CHECK(g.best_streak() == 1);
}

void test_hi_score_storage() {
  MemoryStorage high(1000000);
  Game a(7, &high);
  CHECK(a.hi_score() == 1000000);
  tools::GreedyBot reckless;
  reckless.set_reckless(true);
  while (a.state() != GameState::GameOver) a.tick(reckless.input(a));
  CHECK(high.saves_ == 0);
  CHECK(!a.new_hi_score());

  MemoryStorage empty(0);
  Game b(7, &empty);
  while (b.state() != GameState::GameOver) b.tick(reckless.input(b));
  CHECK(empty.saves_ == 1);
  CHECK(empty.hi_ == b.score());
}

void test_events() {
  tools::GreedyBot bot;

  Game foreseen(11);
  const char* playtest[] = {"#.........", "#........#", "##.#######"};
  start_with_board(foreseen, board_from(playtest, 3));
  CHECK(foreseen.last_event() == GameEvent::Start);
  CHECK(next_event(foreseen, bot) == GameEvent::AsForeseen);

  Game four(11);
  const char* well[] = {"######.###", "######.###", "######.###", "######.###"};
  start_with_board(four, board_from(well, 4));
  CHECK(next_event(four, bot) == GameEvent::FourLines);

  Game over(11);
  tools::GreedyBot reckless;
  reckless.set_reckless(true);
  while (over.state() != GameState::GameOver) over.tick(reckless.input(over));
  CHECK(over.last_event() == GameEvent::GameOver);
}

void test_speech() {
  Game g(11);
  OracleSpeech speech;
  tools::GreedyBot reckless;
  reckless.set_reckless(true);
  while (g.state() != GameState::GameOver) {
    g.tick(reckless.input(g));
    speech.tick(g);
  }
  CHECK(speech.speaking());
  const LineSet game_over = oracle_lines(GameEvent::GameOver);
  bool from_set = false;
  for (int i = 0; i < game_over.count; ++i) from_set |= speech.line() == game_over.lines[i];
  CHECK(from_set);
  for (int i = 0; i < 400; ++i) speech.tick(g);
  CHECK(!speech.speaking());
}

void test_wrap_text() {
  char lines[kSpeechLines][kSpeechChars + 1];
  CHECK(wrap_text("THAT HOLE WAS NOT IN THE PLAN.", lines, kSpeechLines) == 3);
  CHECK(std::string(lines[0]) == "THAT HOLE");
  CHECK(std::string(lines[1]) == "WAS NOT IN");
  CHECK(std::string(lines[2]) == "THE PLAN.");
  CHECK(wrap_text("INTERESTING...", lines, kSpeechLines) == 0);
  CHECK(wrap_text("ONE TWO THREE FOUR FIVE SIX SEVEN", lines, kSpeechLines) == 0);
}

// Every line in oracle_lines.cpp has to fit the bubble.
void test_oracle_lines_fit() {
  const GameEvent events[] = {GameEvent::Start,        GameEvent::AsForeseen, GameEvent::Bold,
                              GameEvent::StreakBroken, GameEvent::LevelUp,    GameEvent::FourLines,
                              GameEvent::GameOver};
  char lines[kSpeechLines][kSpeechChars + 1];
  for (GameEvent e : events) {
    const LineSet set = oracle_lines(e);
    CHECK(set.count > 0);
    for (int i = 0; i < set.count; ++i) {
      if (wrap_text(set.lines[i], lines, kSpeechLines) == 0) {
        std::printf("does not fit the bubble: \"%s\"\n", set.lines[i]);
        ++g_failures;
      }
    }
  }
}

}  // namespace

int main() {
  test_rotations();
  test_kicks();
  test_remove_rows();
  test_oracle_deep_well_gets_i();
  test_oracle_picks_a_clearing_piece();
  test_oracle_prefers_clean_clears();
  test_oracle_fills_lowest_gap();
  test_oracle_empty_board();
  test_spawn_and_gravity();
  test_hard_drop();
  test_das();
  test_pause();
  test_reckless_top_out();
  test_bot_clears_lines();
  test_streak_multiplier();
  test_streak_counts_and_breaks();
  test_hi_score_storage();
  test_events();
  test_speech();
  test_wrap_text();
  test_oracle_lines_fit();
  if (g_failures == 0) std::printf("all tests passed\n");
  return g_failures == 0 ? 0 : 1;
}
