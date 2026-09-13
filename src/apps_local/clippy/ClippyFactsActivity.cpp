#include "ClippyFactsActivity.h"

#include <Memory.h>

#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"
#include "ClippyFactsCore.h"
#include "ClippyFactsScreens.h"

std::unique_ptr<Activity> ClippyFactsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Never a bare new: the firmware is built -fno-exceptions, so a failed
  // allocation aborts rather than throwing.
  return makeUniqueNoThrow<ClippyFactsActivity>(renderer, mappedInput);
}

void ClippyFactsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // Opening on a roll rather than on kFacts[0]: an app that always says the same
  // thing first looks like an app with one fact in it. toybox::seed() rather than
  // millis() by hand, so CROSSPLAY_SEED can pin this for a screenshot.
  fact = clippy::nextFact(fact, toybox::seed());
  requestUpdate();
}

void ClippyFactsActivity::loop() {
  namespace fui = freeink::ui;

  // Back is the only key this app reads, and it names no destination: an app
  // returns to its folder and a folder returns to Home, and leave() is the one
  // place that knows which. On the X4 Pro this is also the left-edge swipe, which
  // MappedInputManager folds into the same release.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0, tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  if (!interactionsReady) return;

  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  // route() gates itself on whether the panel has actually SHOWN this table, so a
  // finger still resting where the shelf row was cannot land on Clippy during the
  // refresh that replaced it.
  if (interactions.route(input).action != clippyui::ActionNextFact) return;

  fact = clippy::nextFact(fact, toybox::seed());
  requestUpdate();
}

void ClippyFactsActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  renderer.clearScreen();
  // readingChromeFaces(): the fact takes the reading cut in the body slot, the
  // caption takes Jersey in the small slot because it is the device speaking
  // rather than the app, and the band keeps the shared display cut so this screen
  // is the same device as the shelf it came from.
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::readingChromeFaces());
  // Frame holds a const DeviceContext&, so this has to outlive it.
  const fui::DeviceContext device = target.deviceContext();
  // The frame never dispatches; the loop task routes against the member table.
  const fui::InputSnapshot noInput{};

  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  clippyui::Model model;
  model.fact = clippy::factText(fact);
  clippyui::buildFacts(surface, model);
  interactionsReady = true;

  // One control on the screen and 24 slots, so this can only fire if the header
  // band starts registering things. Cheap, and the alternative is finding a dead
  // Clippy by tapping him.
  toybox::reportOverflow(interactions, "Clippy Facts");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
