# Zentris

A Tetris parody for the Raspberry Pico 2 W (320x240 ST7789, analog stick, 6 buttons, touch).
The game always hands you the piece you need to clear lines — it just won't tell you where it goes.

Status: milestone 1 — classic Tetris with random pieces (7-bag), running in a desktop simulator.

## Layout

```
src/core/            hardware-independent C++17 game core (shared with the future Pico build)
  pieces.*           tetrominoes, SRS rotations and wall kicks
  board.*            10x20 well (+2 hidden rows), collisions, line clears
  game.*             states, gravity curve, lock delay, DAS, scoring
  render.*           draws the full 320x240 RGB565 frame
  gfx.*              framebuffer, 5x7 bitmap font
  next_window.*      pluggable content for the NEXT box (placeholder: "?")
src/platform/sdl/    desktop simulator (SDL3)
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
| —               | Esc              | quit simulator             |

`--scale N` sets the window scale (default 3).

## Rules (current)

- NES-style gravity curve: 48 frames per row at level 0 down to 1 at level 29; level up every 10 lines.
- Scoring: 40 / 100 / 300 / 1200 × (level + 1), +1 per soft-dropped row, +2 per hard-dropped row.
- 0.5 s lock delay, resets on move/rotate (max 15 per new lowest row).
- No next-piece preview: in Zentris the next piece depends on where you put this one.
