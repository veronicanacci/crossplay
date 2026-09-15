#pragma once

// The facts, the file they can come from, and the one rule about which fact
// comes next.
//
// Freestanding: <cstdint> and nothing else. No renderer, no Arduino, no SDK and
// no SD card -- the card is reached through a read callback, so host-tests/clippy
// can hand this a string and call it a file.
//
// v0.2 adds the card. /clippy/facts.txt, one fact per line, is the whole format:
// a text file, because the person adding facts is the owner of the device and
// not a build. The built-in table stays as the fallback, so an app with no card,
// an empty file or a typo in the path still says something.

#include <cstdint>

namespace clippy {

// ---- the built-in table -----------------------------------------------------

// Sentence case, one sentence, full stop. The shelf label and the header band
// shout; the content does not, because the content is the thing being read.
//
// Kept short deliberately. The fact box is 448px wide at the reading cut, which
// takes roughly thirty characters a line, and a fact that fills the panel is a
// paragraph -- a paragraph is a reader, and the device already has one.
inline constexpr const char* kBuiltinFacts[] = {
    "Octopuses have three hearts.",
    "Sharks are older than trees.",
    "Wombat droppings are cube-shaped.",
    "A day on Venus is longer than a year on Venus.",
    "Bananas are berries. Strawberries are not.",
    "Honey found sealed in Egyptian tombs was still edible.",
    "A shrimp's heart is in its head.",
    "Scotland's national animal is the unicorn.",
};

constexpr int builtinCount() { return static_cast<int>(sizeof(kBuiltinFacts) / sizeof(kBuiltinFacts[0])); }

// The scope says at least five, and a table this size is where the no-repeat
// rule below stops being decoration: with two facts it is an alternation, with
// eight it is a shuffle that never stutters.
static_assert(builtinCount() >= 5, "Clippy Facts ships at least five built-in facts");

// The text at `index`, with an out-of-range index answered rather than trusted:
// a screen builder that draws a null pointer draws nothing, and a panel with a
// picture of Clippy and no sentence looks like a finished screen.
inline const char* builtinText(const int index) {
  if (index < 0 || index >= builtinCount()) return kBuiltinFacts[0];
  return kBuiltinFacts[index];
}

// ---- the rule ---------------------------------------------------------------

// The fact after `current` out of `count`, and never `current` itself.
//
// NOT "roll, and roll again if it repeats". A retry loop has no bound on how
// many rolls it takes, which on a device whose seed is derived from millis()
// is a loop nobody can predict the cost of. This steps forward by 1..count-1
// instead: uniform over exactly the facts that are NOT on the panel, and
// unable to return `current` by construction rather than by checking.
//
// `current` out of range means "no fact yet", which is how the app opens: any
// fact will do, including the first.
inline int nextFact(const int current, const uint32_t roll, const int count) {
  if (count <= 1) return 0;
  if (current < 0 || current >= count) return static_cast<int>(roll % static_cast<uint32_t>(count));
  const int step = 1 + static_cast<int>(roll % static_cast<uint32_t>(count - 1));
  return (current + step) % count;
}

// ---- the card ---------------------------------------------------------------

// Where the app looks. The fork's convention is a folder per app at the card
// root (/trivia, /xkcd, /wikipedia), and a name that says what the file is
// rather than what reads it.
constexpr const char* kFactsPath = "/clippy/facts.txt";

// The index is offsets, not text: the C3 does not have the RAM for the file and
// does not need it, since one tap wants one line. 1024 facts is 6KB of index
// and about three years of daily taps before a repeat is forced.
constexpr int kMaxFacts = 1024;

// The longest line kept. At the reading cut the fact box holds about eight
// lines of thirty characters; a line longer than this would need a ninth and
// would be cut mid-sentence with no mark, so it is skipped at index time and
// counted, rather than drawn wrong.
constexpr int kMaxFactBytes = 200;

// Random-access reads over the file. Returns false on a short or failed read.
// The same shape Connections uses for its pack, so the activity's adapter over
// HalFile is four lines and the host test's adapter over a string is three.
using ReadFn = bool (*)(void* ctx, uint32_t offset, void* dst, uint32_t len);

// One fact per line. Blank lines and lines whose first character is '#' are
// not facts; leading and trailing blanks, and a trailing '\r', are not part
// of one. A UTF-8 byte order mark at the top of the file is what Windows
// Notepad writes and is skipped. That is the entire format.
class FactFile {
 public:
  // Reads the whole file once, 512 bytes at a time, to find where each fact
  // starts. True if at least one usable line was found; on a failed read the
  // index is emptied rather than left half-built.
  bool open(ReadFn read, void* ctx, uint32_t size);

  int count() const { return count_; }
  // Lines that were facts but were not kept: too long, or past kMaxFacts.
  int skipped() const { return skipped_; }

  // Fact `index` into `out`, NUL-terminated. False, and an empty string, when
  // the index is out of range, the buffer too small, or the card says no.
  bool factAt(int index, char* out, int outSize) const;

 private:
  void take(uint32_t start, uint32_t length);

  ReadFn read_ = nullptr;
  void* ctx_ = nullptr;
  uint32_t size_ = 0;
  int count_ = 0;
  int skipped_ = 0;
  uint32_t offsets_[kMaxFacts] = {};
  uint16_t lengths_[kMaxFacts] = {};
};

}  // namespace clippy
