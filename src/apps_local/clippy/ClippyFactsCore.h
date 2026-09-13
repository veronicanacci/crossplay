#pragma once

// The facts, and the one rule about which fact comes next.
//
// Freestanding: <cstdint> and nothing else. No renderer, no Arduino, no SDK, so
// host-tests/clippy compiles it and the rule below is testable without a panel.
//
// v0.1 keeps the whole database here, in the source, on purpose. A card reader
// that loads facts off the SD card is a file format, a parser, a missing-file
// state and an empty-file state, and none of that is the app -- the app is
// Clippy and a sentence.

#include <cstdint>

namespace clippy {

// Sentence case, one sentence, full stop. The shelf label and the header band
// shout; the content does not, because the content is the thing being read.
//
// Kept short deliberately. The fact box is 448px wide at the reading cut, which
// takes roughly sixty characters a line, and a fact that fills the panel is a
// paragraph -- a paragraph is a reader, and the device already has one.
inline constexpr const char* kFacts[] = {
    "Octopuses have three hearts.",
    "Sharks are older than trees.",
    "Wombat droppings are cube-shaped.",
    "A day on Venus is longer than a year on Venus.",
    "Bananas are berries. Strawberries are not.",
    "Honey found sealed in Egyptian tombs was still edible.",
    "A shrimp's heart is in its head.",
    "Scotland's national animal is the unicorn.",
};

constexpr int factCount() { return static_cast<int>(sizeof(kFacts) / sizeof(kFacts[0])); }

// The scope says at least five, and a table this size is where the no-repeat
// rule below stops being decoration: with two facts it is an alternation, with
// eight it is a shuffle that never stutters.
static_assert(factCount() >= 5, "Clippy Facts v0.1 ships at least five facts");

// The fact after `current`, and never `current` itself.
//
// NOT "roll, and roll again if it repeats". A retry loop has no bound on how
// many rolls it takes, which on a device whose seed is derived from millis()
// is a loop nobody can predict the cost of. This steps forward by 1..count-1
// instead: uniform over exactly the facts that are NOT on the panel, and
// unable to return `current` by construction rather than by checking.
//
// `current` out of range means "no fact yet", which is how the app opens: any
// fact will do, including the first.
inline int nextFact(const int current, const uint32_t roll) {
  const int count = factCount();
  if (count <= 1) return 0;
  if (current < 0 || current >= count) return static_cast<int>(roll % static_cast<uint32_t>(count));
  const int step = 1 + static_cast<int>(roll % static_cast<uint32_t>(count - 1));
  return (current + step) % count;
}

// The text at `index`, with an out-of-range index answered rather than trusted:
// a screen builder that draws a null pointer draws nothing, and a panel with a
// picture of Clippy and no sentence looks like a finished screen.
inline const char* factText(const int index) {
  if (index < 0 || index >= factCount()) return kFacts[0];
  return kFacts[index];
}

}  // namespace clippy
