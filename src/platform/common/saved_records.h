#pragma once

#include "core/game.h"
#include "core/storage.h"

namespace zentris {

// Every mode's records as one blob, for a port whose storage keeps blobs (birch's flash).
// Plain data: a change of layout changes its size, which such storage treats as a new blob.
struct SavedRecords {
  Records modes[kModeCount];
};

// Storage over a SavedRecords held in RAM. The port loads it at boot; every save writes the
// whole blob back through `write`.
class BlobStorage : public Storage {
 public:
  using WriteFn = void (*)(const SavedRecords& blob);

  explicit BlobStorage(WriteFn write) : write_(write) {}

  void load(const SavedRecords& blob) { blob_ = blob; }
  void clear() { blob_ = {}; }
  const SavedRecords& blob() const { return blob_; }

  Records load_records(int mode) override {
    return mode >= 0 && mode < kModeCount ? blob_.modes[mode] : Records{};
  }

  void save_records(int mode, const Records& records) override {
    if (mode < 0 || mode >= kModeCount) return;
    blob_.modes[mode] = records;
    if (write_) write_(blob_);
  }

 private:
  SavedRecords blob_{};
  WriteFn write_;
};

}  // namespace zentris
