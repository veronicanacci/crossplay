// Clippy Facts, built and tapped without a device.
//
// The things worth testing here are the things that can silently be wrong:
// that the rect Clippy is DRAWN in is the rect a tap is measured against, that
// the next fact is never the fact already on the panel, and that a text file on
// the card is read into exactly the facts a person typing it would count. All
// are rules a screenshot cannot check -- a picture of the right Clippy in the
// wrong hit target looks exactly like a working app, and a reader that drops
// the last line of a file with no trailing newline looks exactly like one that
// does not.
//
// Deliberately its own suite rather than lines added to host-tests/ui: that
// file is 12k lines shared by every screen in the fork, and this app arrived on
// a machine with no host compiler to check an edit to it with.

#include <FreeInkUI.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../../lib/GfxRenderer/PaintClock.h"
#include "../../src/apps_local/clippy/ClippyFactsCore.h"
#include "../../src/apps_local/clippy/ClippyFactsScreens.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"
#include "../../src/apps_local/ui/ToyboxText.h"

namespace fui = freeink::ui;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    ++checks;                                                             \
    if (!(cond)) {                                                        \
      ++failures;                                                         \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                     \
  } while (0)

// Ten pixels a character and a 20px line, the same stand-in host-tests/ui uses.
// Not the real faces: this suite is about geometry and routing, and a fake with
// round numbers is one whose failures are readable.
constexpr int16_t kCharWidth = 10;
constexpr int16_t kLineHeight = 20;

// The database as shipped, relative to this directory, which is where run.sh
// runs from.
constexpr const char* kShippedFacts = "../../assets_local/clippy/facts.txt";

struct DrawnText {
  fui::Rect rect;
  std::string text;
  fui::TextStyle style;
};

struct DrawnBitmap {
  fui::Rect rect;
  fui::BitmapRef bitmap;
  fui::BitmapMode mode;
};

class FakeTarget final : public fui::DrawTarget {
 public:
  fui::Size measureText(const fui::FontId, const char* text, const fui::TextStyle) const override {
    const int length = text == nullptr ? 0 : static_cast<int>(std::strlen(text));
    return fui::Size{static_cast<int16_t>(length * kCharWidth), kLineHeight};
  }
  int16_t lineHeight(const fui::FontId) const override { return kLineHeight; }
  void fill(const fui::Rect rect, const fui::Paint, const uint8_t = 0, const uint8_t = 0) override {
    fills.push_back(rect);
  }
  void stroke(const fui::Rect rect, const fui::Paint, const uint8_t, const uint8_t = 0,
              const uint8_t = 0) override {
    fills.push_back(rect);
  }
  void line(const fui::Point, const fui::Point, const uint8_t, const fui::Paint) override {}
  void triangle(const fui::Point, const fui::Point, const fui::Point, const fui::Paint) override {}
  void text(const fui::Rect rect, const char* text, const fui::TextStyle style) override {
    texts.push_back(DrawnText{rect, text == nullptr ? std::string() : std::string(text), style});
  }
  void bitmap(const fui::Rect rect, const fui::BitmapRef bitmap, const fui::BitmapMode mode,
              const fui::Paint = fui::Paint::solid(fui::Color::Black),
              const fui::Rotation = fui::Rotation::None) override {
    bitmaps.push_back(DrawnBitmap{rect, bitmap, mode});
  }

  std::vector<DrawnText> texts;
  std::vector<fui::Rect> fills;
  std::vector<DrawnBitmap> bitmaps;
};

// The X4 Pro's app surface: 480x800 portrait, which is what every number in
// ToyboxMetrics.h is an absolute row of.
fui::DeviceContext device() {
  fui::DeviceContext context;
  context.width = 480;
  context.height = 800;
  context.hasTouch = true;
  return context;
}

// One rendered screen, plus a finger.
struct Rendered {
  FakeTarget target;
  toybox::Interactions interactions;

  void build(const char* fact, const char* source = nullptr) {
    const fui::DeviceContext context = device();
    const fui::InputSnapshot noInput{};
    toybox::Frame frame(target, context, noInput, interactions);
    // The one-argument constructor, which is the shared palette. Handing
    // themeTokens() over explicitly would not compile and should not: Screen
    // holds the theme by reference and deletes the rvalue overload.
    toybox::Screen screen(frame);
    clippyui::Model model;
    model.fact = fact;
    model.source = source;
    clippyui::buildFacts(screen, model);
    // A tap is never routed against a table the panel has not shown yet, so a
    // test that never paints can never route. This is that paint.
    paintclock::notePainted();
  }

  fui::ActionEvent tap(const int x, const int y) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(x);
    input.touchY = static_cast<int16_t>(y);
    return interactions.route(input);
  }

  bool drew(const char* needle) const {
    for (const DrawnText& drawn : texts()) {
      if (drawn.text == needle) return true;
    }
    return false;
  }

  const std::vector<DrawnText>& texts() const { return target.texts; }
};

bool sameRect(const fui::Rect& a, const fui::Rect& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

bool insidePanel(const fui::Rect& rect) {
  return rect.x >= 0 && rect.y >= 0 && rect.right() <= 480 && rect.bottom() <= 800;
}

// A string standing in for the card. The same ReadFn shape the activity hands
// FactFile over a HalFile, so the parser under test is the parser on the device.
bool readString(void* ctx, const uint32_t offset, void* dst, const uint32_t len) {
  const auto* bytes = static_cast<const std::string*>(ctx);
  if (static_cast<size_t>(offset) + len > bytes->size()) return false;
  std::memcpy(dst, bytes->data() + offset, len);
  return true;
}

bool readNothing(void*, uint32_t, void*, uint32_t) { return false; }

std::string factAt(const clippy::FactFile& file, const int index) {
  char out[clippy::kMaxFactBytes + 1];
  if (!file.factAt(index, out, static_cast<int>(sizeof(out)))) return std::string("<none>");
  return std::string(out);
}

bool isAscii(const std::string& text) {
  for (const char c : text) {
    if (static_cast<unsigned char>(c) > 126 || static_cast<unsigned char>(c) < 32) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------

void testBuiltinFactsAreAFactTable() {
  CHECK(clippy::builtinCount() >= 5);
  for (int i = 0; i < clippy::builtinCount(); ++i) {
    const char* fact = clippy::kBuiltinFacts[i];
    CHECK(fact != nullptr && fact[0] != '\0');
    // Sentences, not labels: something that ends without a full stop is a title
    // and would want the header band, not the body.
    const size_t length = std::strlen(fact);
    CHECK(length > 8 && fact[length - 1] == '.');
    CHECK(static_cast<int>(length) <= clippy::kMaxFactBytes);
  }
  CHECK(std::strcmp(clippy::builtinText(-1), clippy::kBuiltinFacts[0]) == 0);
  CHECK(std::strcmp(clippy::builtinText(clippy::builtinCount()), clippy::kBuiltinFacts[0]) == 0);
}

void testNextFactNeverRepeats() {
  // At the built-in size and at a card's size, because the rule's arithmetic is
  // modular and a bug in it could hide at one count and not the other.
  for (const int count : {clippy::builtinCount(), 317}) {
    for (int current = 0; current < count; ++current) {
      std::vector<bool> seen(static_cast<size_t>(count), false);
      const uint32_t rolls = count > 64 ? 4096 : 512;
      for (uint32_t roll = 0; roll < rolls; ++roll) {
        const int next = clippy::nextFact(current, roll, count);
        CHECK(next >= 0 && next < count);
        CHECK(next != current);
        seen[static_cast<size_t>(next)] = true;
      }
      // Every other fact is reachable from here: a no-repeat rule that only ever
      // steps to the neighbour would pass the check above and turn a shuffle
      // into a fixed cycle.
      for (int other = 0; other < count; ++other) {
        CHECK(seen[static_cast<size_t>(other)] == (other != current));
      }
    }
    // Opening the app. Any fact will do, including the first.
    for (uint32_t roll = 0; roll < 64; ++roll) {
      const int first = clippy::nextFact(-1, roll, count);
      CHECK(first >= 0 && first < count);
    }
  }
  // Degenerate counts answer rather than divide by zero: a card with one fact
  // on it shows that fact.
  CHECK(clippy::nextFact(0, 7, 1) == 0);
  CHECK(clippy::nextFact(-1, 7, 0) == 0);
}

void testFactFileIndexesWhatAPersonWouldCount() {
  // Every wrinkle a hand-edited text file has: Notepad's byte order mark, CRLF,
  // a comment, blank lines, a line of only spaces, indentation, trailing blanks,
  // a line just over the limit, and no newline after the last line.
  std::string file;
  file += "\xEF\xBB\xBF";
  file += "# Clippy Facts\r\n";
  file += "\r\n";
  file += "Octopuses have three hearts.\r\n";
  file += "   \r\n";
  file += "  Sharks are older than trees.   \n";
  file += "#not a fact\n";
  file += std::string(static_cast<size_t>(clippy::kMaxFactBytes) + 1, 'x') + "\n";
  file += std::string(static_cast<size_t>(clippy::kMaxFactBytes), 'y') + "\n";
  file += "\t#\n";
  file += "Wombat droppings are cube-shaped.";

  clippy::FactFile facts;
  CHECK(facts.open(readString, &file, static_cast<uint32_t>(file.size())));
  CHECK(facts.count() == 4);
  CHECK(facts.skipped() == 1);
  CHECK(factAt(facts, 0) == "Octopuses have three hearts.");
  CHECK(factAt(facts, 1) == "Sharks are older than trees.");
  CHECK(factAt(facts, 2) == std::string(static_cast<size_t>(clippy::kMaxFactBytes), 'y'));
  CHECK(factAt(facts, 3) == "Wombat droppings are cube-shaped.");
  CHECK(factAt(facts, 4) == "<none>");
  CHECK(factAt(facts, -1) == "<none>");

  // A buffer one byte too small is refused, not overrun.
  char tight[8];
  CHECK(!facts.factAt(0, tight, static_cast<int>(sizeof(tight))));
  CHECK(tight[0] == '\0');
}

void testFactFileStopsAtTheCap() {
  std::string file;
  const int lines = clippy::kMaxFacts + 37;
  for (int i = 0; i < lines; ++i) file += "Fact number " + std::to_string(i) + ".\n";
  clippy::FactFile facts;
  CHECK(facts.open(readString, &file, static_cast<uint32_t>(file.size())));
  CHECK(facts.count() == clippy::kMaxFacts);
  CHECK(facts.skipped() == 37);
  CHECK(factAt(facts, clippy::kMaxFacts - 1) == "Fact number " + std::to_string(clippy::kMaxFacts - 1) + ".");
  // Files longer than one chunk, with a fact straddling every chunk boundary
  // there is, come out whole: the index is built across reads, not per read.
  for (int i = 0; i < clippy::kMaxFacts; ++i) {
    CHECK(factAt(facts, i) == "Fact number " + std::to_string(i) + ".");
  }
}

void testFactFileSaysNoToNothing() {
  clippy::FactFile facts;
  std::string empty;
  CHECK(!facts.open(readString, &empty, 0));
  CHECK(facts.count() == 0);

  // A file of comments is a file with nothing in it, which is the case the
  // activity must fall back on rather than index zero facts and divide by them.
  std::string comments = "# one\n# two\n\n";
  CHECK(!facts.open(readString, &comments, static_cast<uint32_t>(comments.size())));
  CHECK(facts.count() == 0);

  // A read that fails empties the index rather than leaving half of one: a card
  // that dies mid-scan must not hand out offsets into a file it cannot read.
  std::string some = "A fact.\nAnother fact.\n";
  CHECK(!facts.open(readNothing, &some, static_cast<uint32_t>(some.size())));
  CHECK(facts.count() == 0);
  CHECK(!facts.open(readString, &some, 0));
  CHECK(!facts.open(nullptr, &some, static_cast<uint32_t>(some.size())));
}

void testTheShippedDatabaseIsWorthShipping() {
  std::ifstream in(kShippedFacts, std::ios::binary);
  CHECK(in.good());
  if (!in.good()) return;
  std::stringstream buffer;
  buffer << in.rdbuf();
  // Not const: ReadFn takes a void* context, the way HalFile is handed over.
  std::string file = buffer.str();

  clippy::FactFile facts;
  CHECK(facts.open(readString, &file, static_cast<uint32_t>(file.size())));
  // A database, not a table with a file extension.
  CHECK(facts.count() >= 150);
  // Nothing in the shipped file is silently dropped: a fact somebody wrote and
  // never sees is the one failure the footer count cannot show.
  CHECK(facts.skipped() == 0);

  const FakeTarget target;
  const clippyui::Layout box = clippyui::layout(device());
  const int lines = box.fact.height / kLineHeight;
  fui::TextStyle style;
  style.font = toybox::kBodyFont;
  style.align = fui::TextAlign::Center;

  std::set<std::string> seen;
  for (int i = 0; i < facts.count(); ++i) {
    const std::string fact = factAt(facts, i);
    // One sentence, said once, in characters the reading face can draw. A
    // sentence ends in a full stop, a question mark or an exclamation mark;
    // a line ending in a letter is a title or a fragment, not a fact.
    CHECK(fact.size() > 8 && (fact.back() == '.' || fact.back() == '!' || fact.back() == '?'));
    CHECK(isAscii(fact));
    CHECK(seen.insert(fact).second);
    CHECK(toybox::fitLines(target, fact.c_str(), box.fact.width, lines, style) == fact);
  }
}

void testLayoutFitsThePanelUnderTheChrome() {
  const clippyui::Layout box = clippyui::layout(device());

  CHECK(box.clippy.width == clippyui::kArtSize);
  CHECK(box.clippy.height == clippyui::kArtSize);
  // Centred on the panel, and clear of the header band, its gap and its rule.
  CHECK(box.clippy.x == (480 - clippyui::kArtSize) / 2);
  CHECK(box.clippy.y >= toybox::kChromeHeight);
  CHECK(box.clippy.y == toybox::kBodyTop);

  // Reading order, with nothing overlapping anything.
  CHECK(box.caption.y >= box.clippy.bottom());
  CHECK(box.rule.y >= box.caption.bottom());
  CHECK(box.fact.y >= box.rule.bottom());
  CHECK(box.footer.y >= box.fact.bottom());
  CHECK(box.fact.height >= kLineHeight * 3);
  CHECK(box.footer.height >= kLineHeight);

  CHECK(insidePanel(box.clippy));
  CHECK(insidePanel(box.caption));
  CHECK(insidePanel(box.rule));
  CHECK(insidePanel(box.fact));
  CHECK(insidePanel(box.footer));
  // The body's own margin, on both sides and at the foot.
  CHECK(box.fact.x == toybox::kMargin);
  CHECK(box.fact.right() == 480 - toybox::kMargin);
  CHECK(box.footer.bottom() <= 800 - toybox::kMargin);
}

void testClippyIsDrawnWhereHeIsTapped() {
  Rendered out;
  out.build(clippy::kBuiltinFacts[0]);
  const clippyui::Layout box = clippyui::layout(device());

  // Drawn: one bitmap, at native size, in the mode that does not resample.
  CHECK(out.target.bitmaps.size() == 1);
  if (!out.target.bitmaps.empty()) {
    const DrawnBitmap& art = out.target.bitmaps.front();
    CHECK(sameRect(art.rect, box.clippy));
    CHECK(art.mode == fui::BitmapMode::Center);
    CHECK(art.bitmap.width == clippyui::kArtSize);
    CHECK(art.bitmap.height == clippyui::kArtSize);
    CHECK(art.bitmap.format == fui::BitmapFormat::Mask1);
  }

  // Tapped: exactly one slot, the same rect, and no second control anywhere.
  int nextFactSlots = 0;
  for (size_t i = 0; i < out.interactions.count(); ++i) {
    const fui::Interaction& slot = out.interactions.data()[i];
    if (slot.action != clippyui::ActionNextFact) continue;
    ++nextFactSlots;
    CHECK(sameRect(slot.rect, box.clippy));
  }
  CHECK(nextFactSlots == 1);

  // And behaviourally, which is the assertion that would survive a rewrite of
  // everything above it.
  CHECK(out.tap(240, box.clippy.y + clippyui::kArtSize / 2).action == clippyui::ActionNextFact);
  CHECK(out.tap(box.clippy.x + 2, box.clippy.y + 2).action == clippyui::ActionNextFact);
  CHECK(out.tap(box.clippy.right() - 2, box.clippy.bottom() - 2).action == clippyui::ActionNextFact);

  // Off Clippy is off. Above him is the body gutter, below him is his caption,
  // beside him is the margin, and under everything is the footer: none of the
  // four is a control.
  CHECK(out.tap(240, box.clippy.y - 4).action == fui::NO_ACTION);
  CHECK(out.tap(240, box.caption.y + 4).action == fui::NO_ACTION);
  CHECK(out.tap(box.clippy.x - 8, box.clippy.y + 40).action == fui::NO_ACTION);
  CHECK(out.tap(240, box.fact.y + 4).action == fui::NO_ACTION);
  CHECK(out.tap(240, box.footer.y + 4).action == fui::NO_ACTION);
}

void testTheScreenSaysWhatItIsAndWhatToDo() {
  Rendered out;
  out.build(clippy::kBuiltinFacts[3], "212 facts from /clippy/facts.txt");

  // The shared chrome, in the shared band. Drawn by toybox::headerBand rather
  // than by this app, which is the point of the assertion.
  CHECK(out.drew("CLIPPY FACTS"));
  CHECK(out.drew("Tap Clippy for another fact"));
  // The fact itself, whole: no ellipsis, and no silent truncation to the box.
  CHECK(out.drew(clippy::kBuiltinFacts[3]));
  // And where it came from, in the footer's box and the caption's cut.
  CHECK(out.drew("212 facts from /clippy/facts.txt"));

  const clippyui::Layout box = clippyui::layout(device());
  bool foundFact = false;
  bool foundSource = false;
  for (const DrawnText& drawn : out.texts()) {
    CHECK(insidePanel(drawn.rect));
    CHECK(drawn.text.find("\xE2\x80\xA6") == std::string::npos);
    if (drawn.text == "212 facts from /clippy/facts.txt") {
      foundSource = true;
      CHECK(sameRect(drawn.rect, box.footer));
      CHECK(drawn.style.font == toybox::kSmallFont);
    }
    if (drawn.text != clippy::kBuiltinFacts[3]) continue;
    foundFact = true;
    CHECK(sameRect(drawn.rect, box.fact));
    // maxLines derived from the box rather than left at its default of ONE,
    // which is what truncates a wrapped sentence with a glyph Jersey lacks.
    CHECK(drawn.style.maxLines > 1);
    CHECK(drawn.style.align == fui::TextAlign::Center);
    CHECK(drawn.style.color == fui::Color::Black);
  }
  CHECK(foundFact);
  CHECK(foundSource);

  for (const fui::Rect& rect : out.target.fills) CHECK(insidePanel(rect));

  // No source, no footer: the model says nullptr and nothing is drawn there.
  Rendered bare;
  bare.build(clippy::kBuiltinFacts[0]);
  for (const DrawnText& drawn : bare.texts()) CHECK(!sameRect(drawn.rect, box.footer));
}

// Every built-in fact, at the real box width, through the fork's own fitting
// rule: if fitLines gives back anything other than the whole sentence then that
// sentence does not fit and something would be dropped on the panel.
void testEveryBuiltinFactFitsItsBox() {
  const FakeTarget target;
  const clippyui::Layout box = clippyui::layout(device());
  const int lines = box.fact.height / kLineHeight;
  CHECK(lines >= 3);
  fui::TextStyle style;
  style.font = toybox::kBodyFont;
  style.align = fui::TextAlign::Center;
  for (int i = 0; i < clippy::builtinCount(); ++i) {
    CHECK(toybox::fitLines(target, clippy::kBuiltinFacts[i], box.fact.width, lines, style) ==
          clippy::kBuiltinFacts[i]);
  }
  // The two footers the activity can print, at the fake's width. The real
  // caption cut is narrower than this fake, so this is the loose end of the
  // check; the tight end is the simulator's own "No glyph for codepoint 8230"
  // in sim-shot.sh, which is what an overflowing footer would trip.
  fui::TextStyle small;
  small.font = toybox::kSmallFont;
  for (const char* footer : {"1024 facts from /clippy/facts.txt", "No /clippy/facts.txt on the card"}) {
    CHECK(toybox::fitLines(target, footer, box.footer.width, 1, small) == footer);
    CHECK(std::strlen(footer) < 40);
  }
}

}  // namespace

int main() {
  testBuiltinFactsAreAFactTable();
  testNextFactNeverRepeats();
  testFactFileIndexesWhatAPersonWouldCount();
  testFactFileStopsAtTheCap();
  testFactFileSaysNoToNothing();
  testTheShippedDatabaseIsWorthShipping();
  testLayoutFitsThePanelUnderTheChrome();
  testClippyIsDrawnWhereHeIsTapped();
  testTheScreenSaysWhatItIsAndWhatToDo();
  testEveryBuiltinFactFitsItsBox();

  // The tree's summary wording, not a nicer one of this suite's own:
  // check.sh counts sub-suites with `grep -c "checks, 0 failed"`, so a suite
  // that words its tally differently runs, passes, and is left out of the
  // "ok (N sub-suite(s))" line -- which is how a suite nobody notices is
  // missing looks exactly like one that was never added.
  std::printf("clippy: %d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
