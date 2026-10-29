// Browser player: the game core compiled to WebAssembly. player.js runs the 60 Hz loop,
// passes in the held Actions from the on-screen buttons and keyboard, and draws the RGBA
// frame this file hands back. Records are kept in localStorage.

#include <emscripten/emscripten.h>

#include <cstdint>

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/oracle_speech.h"
#include "core/render.h"
#include "core/storage.h"

// One key per game mode, "score lines level streak". Storage may be unavailable (private
// browsing, blocked site data); records then just last for the session.
EM_JS(void, js_load_records, (int mode, uint32_t* out), {
  try {
    const s = localStorage.getItem("zentris.records." + mode);
    if (!s) return;
    const v = s.split(" ");
    for (let i = 0; i < 4; ++i) HEAPU32[(out >> 2) + i] = (Number(v[i]) || 0) >>> 0;
  } catch (e) {}
});

EM_JS(void, js_save_records, (int mode, uint32_t score, uint32_t lines, uint32_t level,
                              uint32_t streak), {
  try {
    localStorage.setItem("zentris.records." + mode,
                         [score >>> 0, lines >>> 0, level >>> 0, streak >>> 0].join(" "));
  } catch (e) {}
});

namespace {

class LocalStorage : public zentris::Storage {
 public:
  zentris::Records load_records(int mode) override {
    uint32_t v[4] = {};
    js_load_records(mode, v);
    return {v[0], v[1], v[2], v[3]};
  }

  void save_records(int mode, const zentris::Records& r) override {
    js_save_records(mode, r.score, r.lines, r.level, r.streak);
  }
};

uint16_t pixels[zentris::kScreenW * zentris::kScreenH];
uint8_t rgba[zentris::kScreenW * zentris::kScreenH * 4];

zentris::Framebuffer fb{pixels, zentris::kScreenW, zentris::kScreenH};
LocalStorage storage;
zentris::Game* game = nullptr;
zentris::MysteryNextWindow next;
zentris::OracleSpeech speech;

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE void zentris_init(uint32_t seed) {
  static zentris::Game instance(seed, &storage);
  game = &instance;
}

// Advances one 1/60 s step with the given Action bitmask held.
EMSCRIPTEN_KEEPALIVE void zentris_tick(uint16_t held) {
  game->tick(held);
  next.tick();
  speech.tick(*game);
}

EMSCRIPTEN_KEEPALIVE bool zentris_playing() {
  return game->state() == zentris::GameState::Playing;
}

// Draws the current frame and returns it as 320x240 RGBA bytes.
EMSCRIPTEN_KEEPALIVE uint8_t* zentris_render() {
  zentris::render(*game, next, speech, fb);
  for (int i = 0; i < zentris::kScreenW * zentris::kScreenH; ++i) {
    const uint16_t c = pixels[i];
    const uint8_t r = c >> 11, g = (c >> 5) & 0x3F, b = c & 0x1F;
    rgba[i * 4 + 0] = static_cast<uint8_t>((r << 3) | (r >> 2));
    rgba[i * 4 + 1] = static_cast<uint8_t>((g << 2) | (g >> 4));
    rgba[i * 4 + 2] = static_cast<uint8_t>((b << 3) | (b >> 2));
    rgba[i * 4 + 3] = 255;
  }
  return rgba;
}

}  // extern "C"
