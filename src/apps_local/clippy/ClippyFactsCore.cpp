#include "ClippyFactsCore.h"

namespace clippy {

namespace {

// One SD sector. Reading the file in anything smaller costs a seek per read
// for no saving; anything larger is stack the render task does not have.
constexpr uint32_t kChunk = 512;

// Blank for the purpose of trimming. '\r' is here so a file saved on Windows
// indexes to the same facts as the same file saved anywhere else.
bool isBlank(const uint8_t c) { return c == ' ' || c == '\t' || c == '\r'; }

}  // namespace

void FactFile::take(const uint32_t start, const uint32_t length) {
  if (length == 0) return;
  if (length > static_cast<uint32_t>(kMaxFactBytes) || count_ >= kMaxFacts) {
    ++skipped_;
    return;
  }
  offsets_[count_] = start;
  lengths_[count_] = static_cast<uint16_t>(length);
  ++count_;
}

bool FactFile::open(const ReadFn read, void* ctx, const uint32_t size) {
  read_ = read;
  ctx_ = ctx;
  size_ = size;
  count_ = 0;
  skipped_ = 0;
  if (read == nullptr || size == 0) return false;

  uint8_t chunk[kChunk];
  uint32_t pos = 0;
  if (size >= 3 && read(ctx, 0, chunk, 3) && chunk[0] == 0xEF && chunk[1] == 0xBB && chunk[2] == 0xBF) {
    pos = 3;
  }

  // Per line: where its ink starts, where its ink so far ends, and whether the
  // first ink was a '#'. Blanks never move inkEnd, which is what trims them.
  uint32_t inkStart = 0;
  uint32_t inkEnd = 0;
  bool inked = false;
  bool comment = false;
  while (pos < size) {
    const uint32_t want = size - pos < kChunk ? size - pos : kChunk;
    if (!read(ctx, pos, chunk, want)) {
      count_ = 0;
      skipped_ = 0;
      return false;
    }
    for (uint32_t i = 0; i < want; ++i) {
      const uint8_t c = chunk[i];
      if (c == '\n') {
        if (inked && !comment) take(inkStart, inkEnd - inkStart);
        inked = false;
        comment = false;
        continue;
      }
      if (isBlank(c)) continue;
      if (!inked) {
        inked = true;
        inkStart = pos + i;
        comment = c == '#';
      }
      inkEnd = pos + i + 1;
    }
    pos += want;
  }
  // The last line of a file that does not end in a newline is still a line.
  if (inked && !comment) take(inkStart, inkEnd - inkStart);
  return count_ > 0;
}

bool FactFile::factAt(const int index, char* out, const int outSize) const {
  if (out == nullptr || outSize <= 0) return false;
  out[0] = '\0';
  if (index < 0 || index >= count_ || read_ == nullptr) return false;
  const uint32_t length = lengths_[index];
  if (length >= static_cast<uint32_t>(outSize)) return false;
  if (!read_(ctx_, offsets_[index], out, length)) {
    out[0] = '\0';
    return false;
  }
  out[length] = '\0';
  return true;
}

}  // namespace clippy
