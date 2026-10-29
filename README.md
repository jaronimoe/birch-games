# Zentris

A Tetris parody for the Raspberry Pico 2 W (320x240 ST7789, analog stick, 6 buttons, touch).
The game always hands you the piece you need to clear lines — it just won't tell you where it goes.

Status: fully playable in the desktop simulator and the browser; the Pico port is next
(see [Roadmap](#roadmap)).

## Roadmap

| # | Milestone | Status |
|---|-----------|--------|
| 1 | C++ game core + SDL desktop simulator, classic layout | done |
| 2 | Oracle picks every piece (clean clears first, otherwise fill the lowest gap without a hole) | done |
| 3 | Clean streak multiplier, the oracle's speech bubble, saved records | done |
| — | Game modes (MELLOW / RUSH / ZENLESS) with records per mode | done |
| — | Browser player with on-screen touch controls, published to GitHub Pages | done |
| 4 | Pico 2 W port: ST7789 driver, stick and buttons, records in flash | next — waits for the hardware repo (wiring, pinout, touch controller) |
| 5 | Touch on the device's menus | planned, part of the Pico port |
| 6 | Content for the NEXT window (a micro game or animated pixels) | idea pending |

## The oracle

When a piece is about to spawn, the oracle tries every piece in every rotation and column
(straight drops reachable along the spawn row) on the current board:

1. If any piece can clear lines, you get one of the pieces that clears the most, preferring
   clears that leave no new hole. A stack-shape score (low, flat) also rules out clears that
   leave a tower; the rest are picked at random.
2. Otherwise you get a random piece among those that fill the lowest gap without creating a
   hole (if no piece can avoid a hole, among those that create the fewest).

It decides only after the previous piece landed and its lines cleared, so there is no preview.
Its intended spot is kept secret; press **H** in the simulator to outline it (debug).

## Layout

```
src/core/            hardware-independent C++17 game core (shared with the future Pico build)
  pieces.*           tetrominoes, SRS rotations and wall kicks
  board.*            10x20 well (+2 hidden rows), collisions, line clears
  game.*             states, gravity curve, lock delay, DAS, scoring
  render.*           draws the full 320x240 RGB565 frame
  gfx.*              framebuffer, 5x7 bitmap font
  oracle.*           picks the next piece and remembers where it meant it to go
  oracle_speech.*    what the oracle in the pagoda says, and when
  oracle_lines.cpp   the oracle's lines (edit freely; tests check they fit the bubble)
  storage.h          high-score persistence interface (file in the simulator)
  next_window.*      pluggable content for the NEXT box (placeholder: "?")
src/platform/sdl/    desktop simulator (SDL3)
src/platform/web/    browser player: the core as WebAssembly, records in localStorage
web/                 browser player page: canvas, on-screen buttons, game loop
tools/               greedy bot + headless screenshot tool
tests/               core tests
```

## Build and run

Needs CMake and SDL3 (`brew install cmake sdl3`).

```bash
cmake -S . -B build && cmake --build build -j
```

```bash
./build/zentris_sim
```

```bash
ctest --test-dir build --output-on-failure
```

```bash
mkdir -p shots && ./build/zentris_shot shots
```

## Simulator controls

| Hardware        | Keyboard         | Action                     |
|-----------------|------------------|----------------------------|
| Stick left/right| ← → / A D        | move (hold to auto-repeat) |
| Stick down      | ↓ / S            | soft drop                  |
| Stick click     | Space            | hard drop                  |
| Button          | X / K            | rotate clockwise           |
| Button          | Z / J            | rotate counter-clockwise   |
| Button          | Enter            | start / pause              |
| Button          | P                | pause                      |
| —               | H                | toggle oracle target (debug) |
| —               | Esc              | quit simulator             |

`--scale N` sets the window scale (default 3).

## Rules (current)

- Three modes, picked on the title screen with the stick (up/down): MELLOW (a level every 10
  lines, classic NES pace), RUSH (every 4 lines) and ZENLESS (every line).
- NES-style gravity curve: 48 frames per row at level 0 down to 1 at level 29.
- Scoring: 40 / 100 / 300 / 1200 × (level + 1) × streak multiplier, +1 per soft-dropped row,
  +2 per hard-dropped row.
- Clean streak: every piece placed without creating a hole extends it, a hole resets it.
  Multiplier ×2 from 10, ×3 from 25, ×4 from 50 (shown next to the score).
- The oracle lives in the pagoda and comments on the game in a speech bubble (start, clears
  where it planned, clears it didn't plan, broken streaks, level ups, 4-line clears, game over).
- Each mode keeps its own records for score, lines, level and clean streak, each beaten on its
  own. The title screen shows the selected mode's records; at game over, numbers that beat a
  record turn orange. The simulator saves them to `~/.zentris_hiscore` (one line per mode:
  score lines level streak).
- 0.5 s lock delay, resets on move/rotate (max 15 per new lowest row).
- No next-piece preview: in Zentris the next piece depends on where you put this one.

## Web player

A static page that runs the game in the browser, with on-screen buttons for phones
(D-pad, rotate, drop, start, pause) and the simulator's keys on a keyboard. The oracle's
target outline is not available there. Needs Emscripten (`brew install emscripten`).

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web -j
```

```bash
python3 -m http.server 8123 --directory build-web/web
```

Then open http://localhost:8123 (to try it on a phone, use this Mac's address on the same
network). Pushes to `main` publish it to GitHub Pages via `.github/workflows/pages.yml`;
the repo's Pages source must be set to "GitHub Actions".
