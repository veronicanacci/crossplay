#pragma once

// Clippy Facts on the device. The thin layer: renderer, input, shelf.
//
// No storage. Nothing here is worth a card write -- which fact you were looking
// at is not a place in a book, and reopening on a fresh one is the app working
// rather than the app forgetting.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"

class ClippyFactsActivity final : public Activity {
 public:
  ClippyFactsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Clippy Facts", renderer, mappedInput) {}
  ~ClippyFactsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Which fact is on the panel. -1 is "none yet", which is what onEnter() hands
  // to clippy::nextFact() to open on an arbitrary one.
  int fact = -1;

  toybox::Interactions interactions;
  // Closed across the window where render() has cleared the table and not yet
  // finished refilling it: the loop task and the render task are separate, and a
  // tap routed mid-rebuild is routed against half a screen. The reveal gate in
  // Interactions::route() handles the other half of this problem -- a table the
  // panel has not shown yet -- and does not handle this one.
  bool interactionsReady = false;
};
