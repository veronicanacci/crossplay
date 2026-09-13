#pragma once

// The one screen: Clippy, a rule, a fact.
//
// A free function over a model, FreeInkUI and toybox tokens only. No renderer
// and no activity, which is what lets host-tests/clippy build the real screen
// and route a real tap at it.

#include <FreeInkUI.h>

#include "../ui/ToyboxScreen.h"

namespace clippyui {

namespace fui = freeink::ui;

// Everything one paint needs. One field, and it stays one field: the scope says
// no poses, no expressions and no state that Clippy reacts to, so there is
// nothing else a paint could depend on.
struct Model {
  const char* fact = nullptr;
};

// The only thing on this screen you can tap.
enum : fui::ActionId { ActionNextFact = 1 };

// WHERE EVERYTHING IS, decided once.
//
// The reason this is a struct returned by a function rather than four constants
// read twice: Clippy is both the picture and the button, and those two rects
// have to be the same rect. Minesweeper states the rule for its grid -- hit
// testing must be derived from the pixels, never computed a second time -- and
// gets there with a cellRect()/cellAt() pair because eighty cells do not fit a
// 24-slot interaction table. One target does fit, so this app needs no
// hit-testing code at all: the builder registers `layout.clippy` with the same
// frame it drew `layout.clippy` into, and a drift between the two would have to
// be a drift between a value and itself.
//
// Every number is an ABSOLUTE PANEL ROW (see ToyboxMetrics.h), derived from the
// device rather than from screen.body(), so the layout is a pure function of the
// panel and a test can ask for it without building a Screen.
struct Layout {
  fui::Rect clippy;   // the art, at native size, and the touch target
  fui::Rect caption;  // "Tap Clippy for another fact"
  fui::Rect rule;     // the hairline between chrome and content
  fui::Rect fact;     // the sentence
};

Layout layout(const fui::DeviceContext& device);

// The art's native size, which is also the size it is drawn at. Named here
// because the layout is the only thing allowed to know it: scaling a 1-bit mask
// resamples it with nearest-neighbour drawPixel, and a 2px eyebrow survives that
// or does not depending on the ratio.
constexpr int16_t kArtSize = 240;

void buildFacts(toybox::Screen& screen, const Model& model);

}  // namespace clippyui
