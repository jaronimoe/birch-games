#pragma once

#include <cstdint>

namespace zentris {

// Persistent storage, provided by the platform (a file in the simulator, flash on the Pico).
class Storage {
 public:
  virtual ~Storage() = default;
  virtual uint32_t load_hi_score() = 0;
  virtual void save_hi_score(uint32_t score) = 0;
};

}  // namespace zentris
