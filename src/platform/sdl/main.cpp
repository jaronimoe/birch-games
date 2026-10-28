// Desktop simulator: runs the game core in an SDL window that stands in for the
// Pico's 320x240 ST7789 panel. The keyboard stands in for the stick and buttons.

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/game.h"
#include "core/gfx.h"
#include "core/next_window.h"
#include "core/render.h"

namespace {

struct KeyBinding {
  SDL_Scancode key;
  uint16_t action;
};

// Stick: arrows / WASD. Stick click: Space. Buttons: Z/J, X/K, Enter, P.
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
  zentris::Game game(static_cast<uint32_t>(SDL_GetTicksNS()) ^ 0xA5A5A5A5u);
  zentris::MysteryNextWindow next;

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
    }

    const uint64_t now = SDL_GetTicksNS();
    accumulator += now - last;
    last = now;
    if (accumulator > 5 * kStepNs) accumulator = 5 * kStepNs;  // don't spiral after a stall
    while (accumulator >= kStepNs) {
      game.tick(read_input());
      next.tick();
      accumulator -= kStepNs;
    }

    zentris::render(game, next, fb);
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
