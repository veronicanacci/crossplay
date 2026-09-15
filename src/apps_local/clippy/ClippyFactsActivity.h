#pragma once

// Clippy Facts on the device. The thin layer: renderer, input, shelf, card.
//
// The card is read, never written. Which fact you were looking at is not a place
// in a book, and reopening on a fresh one is the app working rather than the app
// forgetting. What IS read is /clippy/facts.txt, once, into an index of line
// offsets; each tap then costs one seek and one short read.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "ClippyFactsCore.h"

class ClippyFactsActivity final : public Activity {
 public:
  ClippyFactsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clippy Facts", renderer, mappedInput) {}
  ~ClippyFactsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Opens and indexes the card's file, or decides there is none. Sets fromCard
  // and the footer either way, so render() never has to ask twice.
  void openCard();
  // The fact at `fact` into `text`, from the card or from the built-in table.
  void loadFact();
  int factCount() const;

  // Which fact is on the panel. -1 is "none yet", which is what onEnter() hands
  // to clippy::nextFact() to open on an arbitrary one.
  int fact = -1;

  clippy::FactFile facts;
  bool fromCard = false;
  // The sentence on the panel, owned here because a card read has to land
  // somewhere and a built-in fact is copied into the same place so render()
  // reads one buffer whichever it was.
  char text[clippy::kMaxFactBytes + 1] = {};
  char footer[48] = {};

  toybox::Interactions interactions;
  // Closed across the window where render() has cleared the table and not yet
  // finished refilling it: the loop task and the render task are separate, and a
  // tap routed mid-rebuild is routed against half a screen. The reveal gate in
  // Interactions::route() handles the other half of this problem -- a table the
  // panel has not shown yet -- and does not handle this one.
  bool interactionsReady = false;
};
