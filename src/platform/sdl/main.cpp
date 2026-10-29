// Desktop simulator: runs the game core in an SDL window that stands in for the
// Pico's 320x240 ST7789 panel. The keyboard stands in for the stick and buttons.

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/oracle_speech.h"
#include "core/render.h"
#include "core/storage.h"

namespace {

// Keeps the high score in ~/.zentris_hiscore.
class FileStorage : public zentris::Storage {
 public:
  FileStorage() {
    const char* home = std::getenv("HOME");
    path_ = std::string(home ? home : ".") + "/.zentris_hiscore";
  }

  uint32_t load_hi_score() override {
    unsigned long score = 0;
    if (std::FILE* f = std::fopen(path_.c_str(), "r")) {
      if (std::fscanf(f, "%lu", &score) != 1) score = 0;
      std::fclose(f);
    }
    return static_cast<uint32_t>(score);
  }

  void save_hi_score(uint32_t score) override {
    if (std::FILE* f = std::fopen(path_.c_str(), "w")) {
      std::fprintf(f, "%u\n", score);
      std::fclose(f);
    }
  }

 private:
  std::string path_;
};

struct KeyBinding {
  SDL_Scancode key;
  uint16_t action;
};

// Stick: arrows / WASD. Stick click: Space. Buttons: Z/J, X/K, Enter, P.
// Simulator only: H toggles the oracle's target outline, Esc quits.
constexpr KeyBinding kBindings[] = {
    {SDL_SCANCODE_LEFT, zentris::kLeft},       {SDL_SCANCODE_A, zentris::kLeft},
    {SDL_SCANCODE_RIGHT, zentris::kRight},     {SDL_SCANCODE_D, zentris::kRight},
    {SDL_SCANCODE_UP, zentris::kUp},           {SDL_SCANCODE_W, zentris::kUp},
    {SDL_SCANCODE_DOWN, zentris::kDown},       {SDL_SCANCODE_S, zentris::kDown},
    {SDL_SCANCODE_SPACE, zentris::kHardDrop},  {SDL_SCANCODE_X, zentris::kRotateCW},
    {SDL_SCANCODE_K, zentris::kRotateCW},      {SDL_SCANCODE_Z, zentris::kRotateCCW},
    {SDL_SCANCODE_J, zentris::kRotateCCW},     {SDL_SCANCODE_RETURN, zentris::kStart},
    {SDL_SCANCODE_P, zentris::kPause},
};

uint16_t read_input() {
  const bool* keys = SDL_GetKeyboardState(nullptr);
  uint16_t held = 0;
  for (const KeyBinding& b : kBindings)
    if (keys[b.key]) held |= b.action;
  return held;
}

int parse_scale(int argc, char** argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], "--scale") == 0) {
      const int scale = std::atoi(argv[i + 1]);
      if (scale > 0) return scale;
    }
  }
  return 3;
}

}  // namespace

int main(int argc, char** argv) {
  const int scale = parse_scale(argc, argv);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window = SDL_CreateWindow("Zentris", zentris::kScreenW * scale,
                                        zentris::kScreenH * scale, SDL_WINDOW_RESIZABLE);
  SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
  if (!renderer) {
    std::fprintf(stderr, "Window setup failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetRenderVSync(renderer, 1);
  SDL_SetRenderLogicalPresentation(renderer, zentris::kScreenW, zentris::kScreenH,
                                   SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                           SDL_TEXTUREACCESS_STREAMING, zentris::kScreenW,
                                           zentris::kScreenH);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

  static uint16_t pixels[zentris::kScreenW * zentris::kScreenH];
  zentris::Framebuffer fb{pixels, zentris::kScreenW, zentris::kScreenH};
  FileStorage storage;
  zentris::Game game(static_cast<uint32_t>(SDL_GetTicksNS()) ^ 0xA5A5A5A5u, &storage);
  zentris::MysteryNextWindow next;
  zentris::OracleSpeech speech;
  zentris::RenderOptions options;

  constexpr uint64_t kStepNs = 1000000000ull / zentris::kTicksPerSecond;
  uint64_t last = SDL_GetTicksNS();
  uint64_t accumulator = 0;
  bool running = true;

  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) running = false;
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
        running = false;
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_H &&
          !event.key.repeat)
        options.show_oracle_target = !options.show_oracle_target;
    }

    const uint64_t now = SDL_GetTicksNS();
    accumulator += now - last;
    last = now;
    if (accumulator > 5 * kStepNs) accumulator = 5 * kStepNs;  // don't spiral after a stall
    while (accumulator >= kStepNs) {
      game.tick(read_input());
      next.tick();
      speech.tick(game);
      accumulator -= kStepNs;
    }

    zentris::render(game, next, speech, fb, options);
    SDL_UpdateTexture(texture, nullptr, pixels, zentris::kScreenW * sizeof(uint16_t));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
