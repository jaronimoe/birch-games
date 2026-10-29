#pragma once

#include <cstdint>

namespace zentris {

// Best results for one game mode. Each is tracked on its own, so they can come from
// different games.
struct Records {
  uint32_t score = 0;
  uint32_t lines = 0;
  uint32_t level = 0;
  uint32_t streak = 0;
};

// Persistent storage, provided by the platform (a file in the simulator, flash on the Pico).
// `mode` is the GameMode index.
class Storage {
 public:
  virtual ~Storage() = default;
  virtual Records load_records(int mode) = 0;
  virtual void save_records(int mode, const Records& records) = 0;
};

}  // namespace zentris
