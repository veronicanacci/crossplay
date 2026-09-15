#include "ClippyFactsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"
#include "ClippyFactsCore.h"
#include "ClippyFactsScreens.h"

namespace {

// The open handle, alive for as long as the activity is: a fact is one seek and
// one short read, and reopening the file per tap would cost more than the read.
// File-scope, the way Connections keeps its pack, because HalFile is not
// something the freestanding header should know the name of.
HalFile gCard;

bool readCard(void* ctx, const uint32_t offset, void* dst, const uint32_t len) {
  auto* file = static_cast<HalFile*>(ctx);
  if (!file->seek(offset)) return false;
  return file->read(dst, len) == static_cast<int>(len);
}

}  // namespace

std::unique_ptr<Activity> ClippyFactsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Never a bare new: the firmware is built -fno-exceptions, so a failed
  // allocation aborts rather than throwing.
  return makeUniqueNoThrow<ClippyFactsActivity>(renderer, mappedInput);
}

void ClippyFactsActivity::openCard() {
  fromCard = false;
  gCard = HalFile{};
  // Three ways to end up on the built-in table, each logged in its own words,
  // because "no file", "file would not open" and "file has nothing in it" want
  // three different things done about them.
  if (!Storage.exists(clippy::kFactsPath)) {
    LOG_INF("CLIP", "No %s; %d built-in facts", clippy::kFactsPath, clippy::builtinCount());
  } else if (!Storage.openFileForRead("CLIP", clippy::kFactsPath, gCard)) {
    LOG_ERR("CLIP", "%s exists but would not open", clippy::kFactsPath);
  } else if (!facts.open(readCard, &gCard, static_cast<uint32_t>(gCard.fileSize()))) {
    LOG_ERR("CLIP", "%s has no usable facts (%d lines skipped)", clippy::kFactsPath, facts.skipped());
    gCard = HalFile{};
  } else {
    fromCard = true;
    LOG_INF("CLIP", "%d facts from %s, %d lines skipped", facts.count(), clippy::kFactsPath, facts.skipped());
  }
  // Under forty characters either way: the footer is one line of the caption's
  // cut, which holds about that many, and an overflowing footer is truncated
  // with a glyph the face does not carry.
  if (fromCard) {
    std::snprintf(footer, sizeof(footer), "%d facts from %s", facts.count(), clippy::kFactsPath);
  } else {
    std::snprintf(footer, sizeof(footer), "No %s on the card", clippy::kFactsPath);
  }
}

int ClippyFactsActivity::factCount() const { return fromCard ? facts.count() : clippy::builtinCount(); }

void ClippyFactsActivity::loadFact() {
  if (fromCard && facts.factAt(fact, text, static_cast<int>(sizeof(text)))) return;
  // Built in, or a card read that failed mid-session -- the card pulled, a bad
  // sector. The table answers rather than a blank panel, and the index is folded
  // onto it so a card fact number still lands on a real row.
  const int index = fromCard ? fact % clippy::builtinCount() : fact;
  std::snprintf(text, sizeof(text), "%s", clippy::builtinText(index));
}

void ClippyFactsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  openCard();
  // Opening on a roll rather than on the first fact: an app that always says the
  // same thing first looks like an app with one fact in it. toybox::seed() rather
  // than millis() by hand, so CROSSPLAY_SEED can pin this for a screenshot.
  fact = clippy::nextFact(-1, toybox::seed(), factCount());
  loadFact();
  requestUpdate();
}

void ClippyFactsActivity::onExit() {
  gCard = HalFile{};
  fromCard = false;
  Activity::onExit();
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

  fact = clippy::nextFact(fact, toybox::seed(), factCount());
  loadFact();
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
  model.fact = text;
  model.source = footer;
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
