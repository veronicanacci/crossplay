// TO DO, built and driven without a device.
//
// The things worth testing here are the things a screenshot cannot check: that
// a list is complete exactly when its items are and never when it has none;
// that lists keep their place when a neighbour is ticked or pinned; that what
// is written to the card comes back as what was written, and that a damaged
// file costs its damaged lines and nothing else; that a row is as tall as its
// wrapped text and the rows below it move down by the same amount; and that
// the rect the "..." is DRAWN in is the rect a tap is measured against, on a
// row whose body opens the list.

#include <FreeInkUI.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../lib/GfxRenderer/PaintClock.h"
#include "../../src/apps_local/todo/TodoCore.h"
#include "../../src/apps_local/todo/TodoScreens.h"
#include "../../src/apps_local/ui/ToyboxScreen.h"

namespace fui = freeink::ui;

namespace {

int checks = 0;
int failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++checks;                                                     \
    if (!(cond)) {                                                \
      ++failures;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

// Ten pixels a character and a 20px line, the same stand-in host-tests/ui uses.
constexpr int16_t kCharWidth = 10;
constexpr int16_t kLineHeight = 20;

struct DrawnText {
  fui::Rect rect;
  std::string text;
  fui::TextStyle style;
};

struct DrawnBitmap {
  fui::Rect rect;
  fui::BitmapRef bitmap;
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
  void stroke(const fui::Rect rect, const fui::Paint, const uint8_t, const uint8_t = 0, const uint8_t = 0) override {
    strokes.push_back(rect);
  }
  void line(const fui::Point, const fui::Point, const uint8_t, const fui::Paint) override {}
  void triangle(const fui::Point, const fui::Point, const fui::Point, const fui::Paint) override {}
  void text(const fui::Rect rect, const char* text, const fui::TextStyle style) override {
    texts.push_back(DrawnText{rect, text == nullptr ? std::string() : std::string(text), style});
  }
  void bitmap(const fui::Rect rect, const fui::BitmapRef bitmap, const fui::BitmapMode,
              const fui::Paint = fui::Paint::solid(fui::Color::Black),
              const fui::Rotation = fui::Rotation::None) override {
    bitmaps.push_back(DrawnBitmap{rect, bitmap});
  }

  std::vector<DrawnText> texts;
  std::vector<fui::Rect> fills;
  std::vector<fui::Rect> strokes;
  std::vector<DrawnBitmap> bitmaps;
};

// The X4 Pro's app surface: 480x800 portrait.
fui::DeviceContext device() {
  fui::DeviceContext context;
  context.width = 480;
  context.height = 800;
  context.hasTouch = true;
  return context;
}

bool sameRect(const fui::Rect& a, const fui::Rect& b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

bool insidePanel(const fui::Rect& rect) {
  return rect.x >= 0 && rect.y >= 0 && rect.right() <= 480 && rect.bottom() <= 800;
}

bool endsWithEllipsis(const std::string& text) {
  return text.size() > 3 && text.compare(text.size() - 3, 3, "...") == 0;
}

// One rendered screen, plus a finger.
struct Rendered {
  FakeTarget target;
  toybox::Interactions interactions;

  void lists(const todoui::ListsModel& model) {
    const fui::DeviceContext context = device();
    const fui::InputSnapshot noInput{};
    toybox::Frame frame(target, context, noInput, interactions);
    toybox::Screen screen(frame);
    todoui::buildLists(screen, model);
    // A tap is never routed against a table the panel has not shown yet.
    paintclock::notePainted();
  }

  void items(const todoui::ItemsModel& model) {
    const fui::DeviceContext context = device();
    const fui::InputSnapshot noInput{};
    toybox::Frame frame(target, context, noInput, interactions);
    toybox::Screen screen(frame);
    todoui::buildItems(screen, model);
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
    for (const DrawnText& drawn : target.texts) {
      if (drawn.text == needle) return true;
    }
    return false;
  }

  const DrawnText* find(const char* needle) const {
    for (const DrawnText& drawn : target.texts) {
      if (drawn.text == needle) return &drawn;
    }
    return nullptr;
  }

  bool filled(const fui::Rect& rect) const {
    for (const fui::Rect& fill : target.fills) {
      if (sameRect(fill, rect)) return true;
    }
    return false;
  }

  bool stroked(const fui::Rect& rect) const {
    for (const fui::Rect& stroke : target.strokes) {
      if (sameRect(stroke, rect)) return true;
    }
    return false;
  }

  bool blitted(const fui::Rect& rect) const {
    for (const DrawnBitmap& drawn : target.bitmaps) {
      if (sameRect(drawn.rect, rect)) return true;
    }
    return false;
  }

  int slotsFor(const fui::ActionId action) const {
    int n = 0;
    for (size_t i = 0; i < interactions.count(); ++i) {
      if (interactions.data()[i].action == action) ++n;
    }
    return n;
  }

  bool registered(const fui::ActionId action, const int16_t value, const fui::Rect& rect) const {
    for (size_t i = 0; i < interactions.count(); ++i) {
      const fui::Interaction& slot = interactions.data()[i];
      if (slot.action == action && slot.value == value && sameRect(slot.rect, rect)) return true;
    }
    return false;
  }
};

// Where the rows of a page land, computed the way the activity pages and the
// builder draws: each row as tall as rowHeightFor() says, stacked from the
// band's top with a gap between.
std::vector<fui::Rect> listRects(const fui::DrawTarget& target, const todoui::ListRow* rows, const int count) {
  const todoui::Layout box = todoui::layout(device(), false);
  std::vector<fui::Rect> out;
  int16_t y = box.rows.y;
  for (int i = 0; i < count; ++i) {
    const int16_t h = todoui::rowHeightFor(target, toybox::themeTokens(), rows[i].name,
                                           todoui::nameWidth(box.rows, rows[i].pinned), todoui::kNameLines, true);
    out.push_back(fui::makeRect(box.rows.x, y, box.rows.width, h));
    y = static_cast<int16_t>(y + h + todoui::kRowGap);
  }
  return out;
}

std::vector<fui::Rect> itemRects(const fui::DrawTarget& target, const todoui::ItemRow* rows, const int count) {
  const todoui::Layout box = todoui::layout(device(), true);
  std::vector<fui::Rect> out;
  int16_t y = box.rows.y;
  for (int i = 0; i < count; ++i) {
    const int16_t h = todoui::rowHeightFor(target, toybox::themeTokens(), rows[i].text, todoui::itemWidth(box.rows),
                                           todoui::kItemLines, false);
    out.push_back(fui::makeRect(box.rows.x, y, box.rows.width, h));
    y = static_cast<int16_t>(y + h + todoui::kRowGap);
  }
  return out;
}

todo::List listOf(const char* name, const std::vector<std::pair<const char*, bool>>& items, const bool pinned = false) {
  todo::List list;
  list.name = name;
  list.pinned = pinned;
  for (const auto& item : items) {
    todo::Item entry;
    entry.text = item.first;
    entry.complete = item.second;
    list.items.push_back(entry);
  }
  return list;
}

// ---------------------------------------------------------------------------
// The model
// ---------------------------------------------------------------------------

void testCompletionIsDerivedFromTheItems() {
  // 1. An incomplete list is not complete.
  CHECK(!todo::isComplete(listOf("Shopping", {{"Milk", false}, {"Coffee", true}})));
  CHECK(!todo::isComplete(listOf("Shopping", {{"Milk", false}})));
  // 2. A non-empty list with every item complete is complete.
  CHECK(todo::isComplete(listOf("Shopping", {{"Milk", true}, {"Coffee", true}})));
  CHECK(todo::isComplete(listOf("Shopping", {{"Milk", true}})));
  // 3. An empty list is not complete.
  CHECK(!todo::isComplete(listOf("Shopping", {})));
  CHECK(todo::openCount(listOf("Shopping", {{"Milk", false}, {"Coffee", true}, {"Cat food", false}})) == 2);
  CHECK(todo::openCount(listOf("Shopping", {})) == 0);
}

void testTogglingAnItemUpdatesCompletion() {
  // 4. Toggling updates completion both ways.
  todo::Store store;
  const int list = store.addList("Shopping");
  CHECK(list == 0);
  CHECK(store.addItem(list, "Milk") == 0);
  CHECK(store.addItem(list, "Coffee") == 1);
  CHECK(!todo::isComplete(store.lists[list]));

  CHECK(store.toggleItem(list, 0));
  CHECK(!todo::isComplete(store.lists[list]));
  CHECK(store.toggleItem(list, 1));
  CHECK(todo::isComplete(store.lists[list]));
  // X -> O takes the list back out of complete.
  CHECK(store.toggleItem(list, 0));
  CHECK(!todo::isComplete(store.lists[list]));
  CHECK(store.lists[list].items[1].complete);

  // Off the end changes nothing and says so.
  CHECK(!store.toggleItem(list, 2));
  CHECK(!store.toggleItem(list, -1));
  CHECK(!store.toggleItem(1, 0));
}

void testMarkAllSetsEveryItem() {
  todo::Store store;
  const int list = store.addList("Work");
  store.addItem(list, "Report");
  store.addItem(list, "Email");
  store.addItem(list, "Standup");
  store.toggleItem(list, 1);

  // 5. Mark complete marks every item complete.
  CHECK(store.markAll(list, true));
  for (const todo::Item& item : store.lists[list].items) CHECK(item.complete);
  CHECK(todo::isComplete(store.lists[list]));
  // Again is a no-op, and says so, so nothing is saved for nothing.
  CHECK(!store.markAll(list, true));

  // 6. Mark incomplete marks every item incomplete.
  CHECK(store.markAll(list, false));
  for (const todo::Item& item : store.lists[list].items) CHECK(!item.complete);
  CHECK(!todo::isComplete(store.lists[list]));
  CHECK(!store.markAll(list, false));

  // An empty list has nothing to mark, and stays not-complete either way.
  const int empty = store.addList("Empty");
  CHECK(!store.markAll(empty, true));
  CHECK(!todo::isComplete(store.lists[empty]));
  CHECK(!store.markAll(99, true));
}

void testSortingRespectsTheGroups() {
  // 7. Pinned incomplete, incomplete, pinned complete, complete.
  std::vector<todo::List> lists;
  lists.push_back(listOf("done", {{"a", true}}));                // 0: complete
  lists.push_back(listOf("open", {{"a", false}}));               // 1: incomplete
  lists.push_back(listOf("pinned done", {{"a", true}}, true));   // 2: pinned complete
  lists.push_back(listOf("pinned open", {{"a", false}}, true));  // 3: pinned incomplete
  lists.push_back(listOf("empty", {}));                          // 4: incomplete (empty is not done)
  lists.push_back(listOf("pinned empty", {}, true));             // 5: pinned incomplete

  CHECK(todo::sortGroup(lists[3]) == 0);
  CHECK(todo::sortGroup(lists[1]) == 1);
  CHECK(todo::sortGroup(lists[2]) == 2);
  CHECK(todo::sortGroup(lists[0]) == 3);
  CHECK(todo::sortGroup(lists[4]) == 1);
  CHECK(todo::sortGroup(lists[5]) == 0);

  const std::vector<int> order = todo::displayOrder(lists);
  const std::vector<int> want = {3, 5, 1, 4, 2, 0};
  CHECK(order == want);
}

void testOrderIsStableWithinAGroup() {
  // 8. Within a group, creation order holds whatever happens around it.
  todo::Store store;
  store.addList("A");
  store.addList("B");
  store.addList("C");
  store.addList("D");
  for (int i = 0; i < 4; ++i) store.addItem(i, "x");

  std::vector<int> order = todo::displayOrder(store.lists);
  CHECK((order == std::vector<int>{0, 1, 2, 3}));

  // Finishing B sinks it and moves nothing else.
  store.markAll(1, true);
  order = todo::displayOrder(store.lists);
  CHECK((order == std::vector<int>{0, 2, 3, 1}));

  // Pinning D floats it; A and C keep their order; B stays at the bottom.
  store.setPinned(3, true);
  order = todo::displayOrder(store.lists);
  CHECK((order == std::vector<int>{3, 0, 2, 1}));

  // Pinning A too: A was created before D, so A leads the pinned group.
  store.setPinned(0, true);
  order = todo::displayOrder(store.lists);
  CHECK((order == std::vector<int>{0, 3, 2, 1}));

  // Unfinishing B returns it to the loose group AHEAD of C: B was created
  // before C. Stable means creation order, not "where it last sat".
  store.markAll(1, false);
  order = todo::displayOrder(store.lists);
  CHECK((order == std::vector<int>{0, 3, 1, 2}));

  // Sorting twice is sorting once.
  CHECK(todo::displayOrder(store.lists) == order);
}

void testDeletingRemovesTheRightList() {
  // 9. Delete removes exactly the list asked for.
  todo::Store store;
  store.addList("A");
  store.addList("B");
  store.addList("C");
  store.addItem(1, "b only");
  CHECK(store.removeList(1));
  CHECK(store.lists.size() == 2);
  CHECK(store.lists[0].name == "A");
  CHECK(store.lists[1].name == "C");
  CHECK(store.lists[1].items.empty());
  CHECK(!store.removeList(2));
  CHECK(!store.removeList(-1));
  CHECK(store.lists.size() == 2);
}

void testRemovingItemsTakesExactlyTheMarkedOnes() {
  todo::Store store;
  const int list = store.addList("Shopping");
  for (const char* text : {"Milk", "Coffee", "Cat food", "Bread"}) store.addItem(list, text);
  store.toggleItem(list, 1);

  // Marks on the first and third; the others keep their place and their state.
  std::vector<bool> doomed = {true, false, true, false};
  CHECK(store.removeItems(list, doomed) == 2);
  CHECK(store.lists[list].items.size() == 2);
  CHECK(store.lists[list].items[0].text == "Coffee");
  CHECK(store.lists[list].items[0].complete);
  CHECK(store.lists[list].items[1].text == "Bread");
  CHECK(!store.lists[list].items[1].complete);

  // No marks, nothing goes; marks past the end are not read.
  CHECK(store.removeItems(list, {}) == 0);
  CHECK(store.removeItems(list, {false, false, true, true, true}) == 0);
  CHECK(store.lists[list].items.size() == 2);
  // Every mark set empties the list, which is then not complete.
  CHECK(store.removeItems(list, {true, true}) == 2);
  CHECK(store.lists[list].items.empty());
  CHECK(!todo::isComplete(store.lists[list]));
  CHECK(store.removeItems(3, {true}) == 0);
}

void testTheFileRoundTrips() {
  // 10. Encode then decode is the identity.
  std::vector<todo::List> lists;
  lists.push_back(listOf("Shopping", {{"Milk", false}, {"Coffee", true}}, true));
  lists.push_back(listOf("Work", {}));
  lists.push_back(listOf("Bars | and | bars", {{"a | b", true}, {"#not a comment", false}}));
  lists.push_back(listOf("Caff\xC3\xA8", {{"Pi\xC3\xB9 latte", false}}));

  const std::string file = todo::encode(lists);
  CHECK(file.rfind("# CrossPlay TO DO v1\n", 0) == 0);
  CHECK(file.find("L|1|Shopping\nI|0|Milk\nI|1|Coffee\nL|0|Work\n") != std::string::npos);

  std::vector<todo::List> back;
  todo::decode(file, back);
  CHECK(back.size() == lists.size());
  for (size_t i = 0; i < lists.size() && i < back.size(); ++i) {
    CHECK(back[i].name == lists[i].name);
    CHECK(back[i].pinned == lists[i].pinned);
    CHECK(back[i].items.size() == lists[i].items.size());
    for (size_t j = 0; j < lists[i].items.size() && j < back[i].items.size(); ++j) {
      CHECK(back[i].items[j].text == lists[i].items[j].text);
      CHECK(back[i].items[j].complete == lists[i].items[j].complete);
    }
  }
  // Completion survives the trip because it was never stored: it is the items.
  CHECK(!todo::isComplete(back[0]));
  CHECK(!todo::isComplete(back[1]));

  // Nothing at all decodes to no lists, which is how a card without the file
  // starts.
  std::vector<todo::List> none;
  todo::decode("", none);
  CHECK(none.empty());
  todo::decode(todo::encode(none), none);
  CHECK(none.empty());
}

void testADamagedFileCostsOnlyItsDamagedLines() {
  // Every wrinkle a hand-edited or half-written file has.
  std::string file;
  file += "# CrossPlay TO DO v1\r\n";
  file += "I|0|orphan before any list\n";  // no list to join
  file += "L|1|Shopping\r\n";              // CRLF
  file += "I|0|Milk\n";
  file += "I|2|bad flag\n";  // not 0 or 1
  file += "I|1|\n";          // empty text
  file += "I|1|   \n";       // blank text
  file += "garbage\n";
  file += "\n";
  file += "X|0|unknown kind\n";
  file += "I|1|Coffee\n";
  file += "L|0|   \n";                           // a list with no name
  file += "I|0|would join the nameless list\n";  // joins Shopping instead: the nameless one was never made
  file += "L|0|Work";                            // no trailing newline

  std::vector<todo::List> lists;
  todo::decode(file, lists);
  CHECK(lists.size() == 2);
  if (lists.size() == 2) {
    CHECK(lists[0].name == "Shopping");
    CHECK(lists[0].pinned);
    CHECK(lists[0].items.size() == 3);
    if (lists[0].items.size() == 3) {
      CHECK(lists[0].items[0].text == "Milk");
      CHECK(!lists[0].items[0].complete);
      CHECK(lists[0].items[1].text == "Coffee");
      CHECK(lists[0].items[1].complete);
      CHECK(lists[0].items[2].text == "would join the nameless list");
    }
    CHECK(lists[1].name == "Work");
    CHECK(lists[1].items.empty());
  }

  // Past the limits: the file is cut at the caps, and items of a list that was
  // not kept do not land on the last list that was.
  std::string big;
  for (int i = 0; i < todo::kMaxLists + 3; ++i) {
    big += "L|0|List " + std::to_string(i) + "\n";
    for (int j = 0; j < todo::kMaxItems + 2; ++j) big += "I|0|item\n";
  }
  todo::decode(big, lists);
  CHECK(static_cast<int>(lists.size()) == todo::kMaxLists);
  for (const todo::List& list : lists) CHECK(static_cast<int>(list.items.size()) == todo::kMaxItems);
}

void testBlankNamesAreRefused() {
  todo::Store store;
  // 11. Empty-name lists are not created.
  CHECK(store.addList("") == -1);
  CHECK(store.addList("   ") == -1);
  CHECK(store.addList("\t\r\n") == -1);
  CHECK(store.lists.empty());
  // Trimmed, and line breaks flattened, because the file is one entry a line.
  CHECK(store.addList("  Shopping  ") == 0);
  CHECK(store.lists[0].name == "Shopping");
  CHECK(store.addList("Two\nlines") == 1);
  CHECK(store.lists[1].name == "Two lines");

  // 12. Empty-text items are not created.
  CHECK(store.addItem(0, "") == -1);
  CHECK(store.addItem(0, "  ") == -1);
  CHECK(store.lists[0].items.empty());
  CHECK(store.addItem(0, " Milk ") == 0);
  CHECK(store.lists[0].items[0].text == "Milk");
  CHECK(store.addItem(5, "Nowhere") == -1);

  // The caps hold, and say so.
  todo::Store full;
  for (int i = 0; i < todo::kMaxLists; ++i) CHECK(full.addList("L") == i);
  CHECK(full.addList("one more") == -1);
  for (int i = 0; i < todo::kMaxItems; ++i) CHECK(full.addItem(0, "i") == i);
  CHECK(full.addItem(0, "one more") == -1);
}

void testPagingStopsAtBothEnds() {
  CHECK(todo::pageStep(0, 3, 1) == 1);
  CHECK(todo::pageStep(2, 3, 1) == 2);
  CHECK(todo::pageStep(0, 3, -1) == 0);
  CHECK(todo::pageStep(5, 3, 0) == 2);
  CHECK(todo::pageStep(0, 1, 1) == 0);
}

// ---------------------------------------------------------------------------
// The screens
// ---------------------------------------------------------------------------

void testLayoutFitsThePanelUnderTheChrome() {
  for (const bool status : {false, true}) {
    const todoui::Layout box = todoui::layout(device(), status);
    CHECK(box.rows.x == toybox::kMargin);
    CHECK(box.rows.right() == 480 - toybox::kMargin);
    CHECK(box.action.bottom() == 800 - toybox::kMargin);
    CHECK(box.action.height == toybox::kPillHeight);
    CHECK(box.rows.bottom() < box.action.y);
    CHECK(insidePanel(box.rows));
    CHECK(insidePanel(box.action));
    if (status) {
      CHECK(box.status.y == toybox::kBodyTop);
      CHECK(box.status.height > 0);
      CHECK(box.rows.y > box.status.bottom());
    } else {
      CHECK(box.status.height == 0);
      CHECK(box.rows.y == toybox::kBodyTop);
    }
  }
  // The foot: no bin on the main screen and the pill runs the whole width; on
  // a list the bin is a square at the left and the pill takes the rest.
  const todoui::Layout main = todoui::layout(device(), false);
  CHECK(main.bin.width == 0);
  CHECK(main.action.x == toybox::kMargin);
  CHECK(main.action.width == 480 - 2 * toybox::kMargin);
  const todoui::Layout list = todoui::layout(device(), true);
  CHECK(list.bin.width == toybox::kPillHeight);
  CHECK(list.bin.height == toybox::kPillHeight);
  CHECK(list.bin.x == toybox::kMargin);
  CHECK(list.bin.y == list.action.y);
  CHECK(list.action.x == list.bin.right() + toybox::kGutter);
  CHECK(list.action.right() == 480 - toybox::kMargin);
  CHECK(list.action.width >= 300);

  // A name has less room than an item: the "..." takes some, the pin more.
  const todoui::Layout box = todoui::layout(device(), false);
  CHECK(todoui::nameWidth(box.rows, false) > todoui::nameWidth(box.rows, true));
  CHECK(todoui::itemWidth(box.rows) > todoui::nameWidth(box.rows, false));
  CHECK(todoui::nameWidth(box.rows, true) >= 280);
}

void testRowsAreAsTallAsTheirText() {
  const FakeTarget target;
  const fui::ThemeTokens& tokens = toybox::themeTokens();
  const todoui::Layout box = todoui::layout(device(), false);
  const int16_t width = todoui::nameWidth(box.rows, false);

  // Short text: the theme's row, whatever the text needs.
  CHECK(todoui::rowHeightFor(target, tokens, "Shopping", width, todoui::kNameLines, true) == tokens.rowHeight);
  CHECK(todoui::rowHeightFor(target, tokens, "Milk", todoui::itemWidth(box.rows), todoui::kItemLines, false) ==
        tokens.rowHeight);

  // A name the width will not hold on one line takes two, and the row grows by
  // exactly the extra line.
  const std::string twoLines(static_cast<size_t>(width / kCharWidth + 5), 'x');
  const std::string spaced = twoLines.substr(0, 20) + " " + twoLines.substr(21);
  const int16_t two = todoui::rowHeightFor(target, tokens, spaced.c_str(), width, todoui::kNameLines, true);
  CHECK(two > tokens.rowHeight);
  CHECK(two == kLineHeight * 2 + kLineHeight + 8);

  // Past the line budget, the row stops growing: the text is cut instead.
  std::string many;
  for (int i = 0; i < 40; ++i) many += "word ";
  CHECK(todoui::rowHeightFor(target, tokens, many.c_str(), width, todoui::kNameLines, true) ==
        kLineHeight * todoui::kNameLines + kLineHeight + 8);
  const int16_t itemW = todoui::itemWidth(box.rows);
  CHECK(todoui::rowHeightFor(target, tokens, many.c_str(), itemW, todoui::kItemLines, false) ==
        kLineHeight * todoui::kItemLines + 8);
  // Anything the keyboard lets through wraps whole: the longest name it allows,
  // at the narrowest width a row gives one, is never cut.
  std::string longest;
  while (static_cast<int>(longest.size()) < todo::kMaxTextChars) longest += longest.empty() ? "word" : " word";
  longest.resize(static_cast<size_t>(todo::kMaxTextChars));
  const int16_t narrow = todoui::nameWidth(box.rows, true);
  CHECK(static_cast<int>(longest.size()) * kCharWidth <= narrow * todoui::kNameLines);
}

void testPagesFillGreedily() {
  const todoui::Layout box = todoui::layout(device(), false);
  const int16_t row = toybox::themeTokens().rowHeight;
  std::vector<int16_t> uniform(20, row);
  const std::vector<int> starts = todoui::pageStarts(box.rows, uniform.data(), 20);
  // The first page holds as many theme rows as fit with gaps between, and the
  // last row on it is inside the band.
  CHECK(starts.size() >= 3);
  const int perPage = starts[1];
  CHECK(perPage >= 6);
  CHECK(perPage <= todoui::kMaxRowsOnPage);
  CHECK(perPage * row + (perPage - 1) * todoui::kRowGap <= box.rows.height);
  CHECK((perPage + 1) * row + perPage * todoui::kRowGap > box.rows.height);
  CHECK(starts[2] == 2 * perPage);
  CHECK(todoui::pageOf(starts, 0) == 0);
  CHECK(todoui::pageOf(starts, perPage - 1) == 0);
  CHECK(todoui::pageOf(starts, perPage) == 1);
  CHECK(todoui::pageOf(starts, 19) == static_cast<int>(starts.size()) - 1);

  // Nothing is one page, not none.
  CHECK(todoui::pageStarts(box.rows, nullptr, 0).size() == 1);

  // A tall row costs its page room: three tall rows and a short one.
  const int16_t heights[] = {static_cast<int16_t>(box.rows.height / 2), static_cast<int16_t>(box.rows.height / 2),
                             static_cast<int16_t>(box.rows.height / 2), row};
  const std::vector<int> mixed = todoui::pageStarts(box.rows, heights, 4);
  CHECK((mixed == std::vector<int>{0, 1, 2}) || (mixed == std::vector<int>{0, 1, 2, 3}));

  // A row taller than the whole band still gets a page rather than being lost.
  const int16_t giant[] = {row, static_cast<int16_t>(box.rows.height + 50), row};
  CHECK((todoui::pageStarts(box.rows, giant, 3) == std::vector<int>{0, 1, 2}));
}

void testAListRowOpensAndItsDotsDoNot() {
  todoui::ListRow rows[3];
  rows[0].name = "Shopping";
  rows[0].open = 3;
  rows[0].total = 5;
  rows[0].pinned = true;
  rows[0].value = 4;
  rows[1].name = "Work";
  rows[1].open = 0;
  rows[1].total = 2;
  rows[1].complete = true;
  rows[1].value = 1;
  rows[2].name = "Holiday packing";
  rows[2].total = 0;
  rows[2].value = 7;
  todoui::ListsModel model;
  model.rows = rows;
  model.count = 3;

  Rendered out;
  out.lists(model);
  const todoui::Layout box = todoui::layout(device(), false);
  const std::vector<fui::Rect> rects = listRects(out.target, rows, 3);

  CHECK(out.drew("TO DO"));
  CHECK(out.drew("NEW LIST"));
  CHECK(out.drew("Shopping"));
  CHECK(out.drew("Work"));
  CHECK(out.drew("Holiday packing"));
  CHECK(out.drew("3 OPEN"));
  CHECK(out.drew("COMPLETE"));
  CHECK(out.drew("NO ITEMS"));
  CHECK(!out.drew("No lists yet."));
  // One page: no counter.
  CHECK(!out.drew("1/1"));

  for (int i = 0; i < 3; ++i) {
    const fui::Rect row = rects[static_cast<size_t>(i)];
    const fui::Rect more = todoui::moreRect(row);
    const fui::Rect markBox = todoui::markRect(row);
    CHECK(insidePanel(row));
    CHECK(row.bottom() <= box.rows.bottom());
    CHECK(row.height == toybox::themeTokens().rowHeight);

    // The row's frame is drawn where the row is registered.
    CHECK(out.stroked(row));
    CHECK(out.registered(todoui::ActionOpenList, rows[i].value, row));
    // The "..." is registered against the rect it was drawn into.
    CHECK(out.registered(todoui::ActionListMenu, rows[i].value, more));
    const fui::Rect dots = fui::makeRect(static_cast<int16_t>(more.x + (more.width - toybox::kIconSize) / 2),
                                         static_cast<int16_t>(more.y + (more.height - toybox::kIconSize) / 2),
                                         toybox::kIconSize, toybox::kIconSize);
    CHECK(out.blitted(dots));

    // The mark: an outline for open, a slab for done.
    if (rows[i].complete) {
      CHECK(out.filled(markBox));
      CHECK(!out.stroked(markBox));
    } else {
      CHECK(out.stroked(markBox));
      CHECK(!out.filled(markBox));
    }

    // The pin, only on the pinned row.
    CHECK(out.blitted(todoui::pinRect(row)) == rows[i].pinned);

    // The name sits inside the row, clear of the pin and the dots.
    const DrawnText* name = out.find(rows[i].name);
    CHECK(name != nullptr);
    if (name != nullptr) {
      CHECK(name->rect.y >= row.y);
      CHECK(name->rect.bottom() <= row.bottom());
      CHECK(name->rect.right() <= (rows[i].pinned ? todoui::pinRect(row).x : more.x));
      CHECK(name->rect.x > markBox.right());
    }

    // And behaviourally: the body opens, the dots do not, and both carry the
    // STORE index rather than the row's.
    const int midY = row.y + row.height / 2;
    fui::ActionEvent body = out.tap(row.x + 100, midY);
    CHECK(body.action == todoui::ActionOpenList);
    CHECK(body.value == rows[i].value);
    fui::ActionEvent nameEnd = out.tap(more.x - 4, midY);
    CHECK(nameEnd.action == todoui::ActionOpenList);
    fui::ActionEvent dotsTap = out.tap(more.x + more.width / 2, midY);
    CHECK(dotsTap.action == todoui::ActionListMenu);
    CHECK(dotsTap.value == rows[i].value);
    fui::ActionEvent edge = out.tap(more.x + 1, row.y + 1);
    CHECK(edge.action == todoui::ActionListMenu);
  }

  // Two targets a row and the pill, nothing else. A fourth row's worth of
  // slots would mean something is registering twice.
  CHECK(out.slotsFor(todoui::ActionOpenList) == 3);
  CHECK(out.slotsFor(todoui::ActionListMenu) == 3);
  CHECK(out.slotsFor(todoui::ActionNewList) == 1);

  // The pill, where it was drawn.
  const fui::ActionEvent pill = out.tap(240, box.action.y + box.action.height / 2);
  CHECK(pill.action == todoui::ActionNewList);
  // Between the last row and the pill is nobody's.
  CHECK(!out.tap(240, (rects[2].bottom() + box.action.y) / 2));
  // The gap between two rows is nobody's either.
  CHECK(!out.tap(240, rects[0].bottom() + 1));
  // The band is nobody's.
  CHECK(!out.tap(240, 40));
}

void testALongNameWrapsAndMovesTheRowsBelowIt() {
  const todoui::Layout box = todoui::layout(device(), false);
  // Long enough for two lines at the fake's ten pixels a character, and made of
  // words so there is somewhere to break.
  std::string longName;
  while (static_cast<int>(longName.size()) * kCharWidth <= todoui::nameWidth(box.rows, true)) longName += "long ";
  longName += "name";
  std::string endless;
  for (int i = 0; i < 40; ++i) endless += "word ";

  todoui::ListRow rows[3];
  rows[0].name = longName.c_str();
  rows[0].pinned = true;
  rows[0].total = 1;
  rows[0].open = 1;
  rows[0].value = 0;
  rows[1].name = "Short";
  rows[1].total = 1;
  rows[1].open = 1;
  rows[1].value = 1;
  rows[2].name = endless.c_str();
  rows[2].total = 1;
  rows[2].open = 1;
  rows[2].value = 2;
  todoui::ListsModel model;
  model.rows = rows;
  model.count = 3;

  Rendered out;
  out.lists(model);
  const std::vector<fui::Rect> rects = listRects(out.target, rows, 3);

  // The first row grew by a line; the second sits below it, not where a
  // fixed-height row would have put it.
  CHECK(rects[0].height > rects[1].height);
  CHECK(rects[0].height == kLineHeight * 3 + 8);
  CHECK(rects[1].y == rects[0].bottom() + todoui::kRowGap);
  for (int i = 0; i < 3; ++i) {
    CHECK(out.stroked(rects[static_cast<size_t>(i)]));
    CHECK(out.registered(todoui::ActionOpenList, rows[i].value, rects[static_cast<size_t>(i)]));
    CHECK(out.registered(todoui::ActionListMenu, rows[i].value, todoui::moreRect(rects[static_cast<size_t>(i)])));
  }

  // The long name is drawn WHOLE, on two lines, inside its row: no ellipsis.
  const DrawnText* whole = out.find(longName.c_str());
  CHECK(whole != nullptr);
  if (whole != nullptr) {
    CHECK(whole->style.maxLines == 2);
    CHECK(whole->rect.height == kLineHeight * 2);
    CHECK(whole->rect.y >= rects[0].y);
    CHECK(whole->rect.bottom() <= rects[0].bottom());
    CHECK(whole->rect.width <= todoui::nameWidth(box.rows, true));
  }
  // The endless one is cut with the fork's ellipsis at the last line it may
  // take, and its row is as tall as that many lines and no taller.
  bool cut = false;
  for (const DrawnText& drawn : out.target.texts) {
    if (drawn.text.rfind("word word", 0) == 0) {
      cut = endsWithEllipsis(drawn.text);
      CHECK(drawn.style.maxLines == todoui::kNameLines);
    }
  }
  CHECK(cut);
  CHECK(rects[2].height == kLineHeight * todoui::kNameLines + kLineHeight + 8);

  // Taps land on the rows where they are now, not where fixed rows would be.
  CHECK(out.tap(200, rects[1].y + 5).value == 1);
  CHECK(out.tap(200, rects[0].bottom() - 5).value == 0);
  CHECK(out.tap(200, rects[2].y + rects[2].height / 2).value == 2);
}

void testAFullPageOfListsFitsTheTable() {
  const todoui::Layout box = todoui::layout(device(), false);
  const int16_t row = toybox::themeTokens().rowHeight;
  std::vector<int16_t> uniform(todoui::kMaxRowsOnPage, row);
  const int perPage = todoui::pageStarts(box.rows, uniform.data(), todoui::kMaxRowsOnPage)[1];
  std::vector<todoui::ListRow> rows(static_cast<size_t>(perPage));
  for (int i = 0; i < perPage; ++i) {
    rows[static_cast<size_t>(i)].name = "A list";
    rows[static_cast<size_t>(i)].total = 1;
    rows[static_cast<size_t>(i)].open = 1;
    rows[static_cast<size_t>(i)].pinned = i % 2 == 0;
    rows[static_cast<size_t>(i)].value = static_cast<int16_t>(i);
  }
  todoui::ListsModel model;
  model.rows = rows.data();
  model.count = perPage;
  model.page = 1;
  model.pageCount = 3;

  Rendered out;
  out.lists(model);
  CHECK(out.slotsFor(todoui::ActionOpenList) == perPage);
  CHECK(out.slotsFor(todoui::ActionListMenu) == perPage);
  CHECK(out.slotsFor(todoui::ActionNewList) == 1);
  CHECK(out.interactions.count() == static_cast<size_t>(perPage * 2 + 1));
  CHECK(out.interactions.count() <= toybox::kMaxInteractions);
  CHECK(out.drew("2/3"));
  // The last row is inside the band and clear of the pill.
  const std::vector<fui::Rect> rects = listRects(out.target, rows.data(), perPage);
  CHECK(rects.back().bottom() <= box.rows.bottom());
  CHECK(out.stroked(rects.back()));
}

void testTheEmptyShelfSaysSo() {
  todoui::ListsModel model;
  Rendered out;
  out.lists(model);
  CHECK(out.drew("No lists yet."));
  CHECK(out.drew("NEW LIST"));
  CHECK(out.slotsFor(todoui::ActionOpenList) == 0);
  CHECK(out.slotsFor(todoui::ActionListMenu) == 0);
  const todoui::Layout box = todoui::layout(device(), false);
  CHECK(out.tap(240, box.action.y + 10).action == todoui::ActionNewList);
  CHECK(!out.tap(240, box.rows.y + 10));
}

void testAnItemRowTogglesWhereverItIsTapped() {
  const todoui::Layout box = todoui::layout(device(), true);
  std::string threeLines;
  while (static_cast<int>(threeLines.size()) * kCharWidth <= todoui::itemWidth(box.rows) * 2) threeLines += "buy ";
  threeLines += "everything";

  todoui::ItemRow rows[4];
  rows[0].text = "Buy milk";
  rows[0].value = 0;
  rows[1].text = "Buy coffee";
  rows[1].complete = true;
  rows[1].value = 1;
  rows[2].text = threeLines.c_str();
  rows[2].value = 2;
  rows[3].text = "Cat food";
  rows[3].value = 3;
  todoui::ItemsModel model;
  model.title = "SHOPPING";
  model.rows = rows;
  model.count = 4;

  Rendered out;
  out.items(model);
  const std::vector<fui::Rect> rects = itemRects(out.target, rows, 4);

  CHECK(out.drew("SHOPPING"));
  CHECK(out.drew("ADD ITEM"));
  CHECK(out.drew("Buy milk"));
  CHECK(out.drew("Buy coffee"));
  CHECK(out.drew("Cat food"));
  CHECK(!out.drew("COMPLETE"));
  CHECK(!out.drew("No items yet."));

  // The long item wraps to three lines, whole, and the row after it moves down.
  const DrawnText* whole = out.find(threeLines.c_str());
  CHECK(whole != nullptr);
  if (whole != nullptr) {
    CHECK(whole->style.maxLines == 3);
    CHECK(whole->rect.height == kLineHeight * 3);
  }
  CHECK(rects[2].height == kLineHeight * 3 + 8);
  CHECK(rects[3].y == rects[2].bottom() + todoui::kRowGap);
  CHECK(rects[0].height == toybox::themeTokens().rowHeight);

  for (int i = 0; i < 4; ++i) {
    const fui::Rect row = rects[static_cast<size_t>(i)];
    const fui::Rect markBox = todoui::markRect(row);
    CHECK(out.stroked(row));
    CHECK(rows[i].complete ? out.filled(markBox) : out.stroked(markBox));
    const int midY = row.y + row.height / 2;
    // The whole row, edge to edge: the box, the text, the empty right.
    for (const int x : {markBox.x + 2, row.x + 120, row.right() - 6}) {
      const fui::ActionEvent tap = out.tap(x, midY);
      CHECK(tap.action == todoui::ActionToggleItem);
      CHECK(tap.value == rows[i].value);
    }
  }
  CHECK(out.slotsFor(todoui::ActionToggleItem) == 4);
  CHECK(out.slotsFor(todoui::ActionListMenu) == 0);
  CHECK(out.tap(240, box.action.y + 10).action == todoui::ActionAddItem);
  // The status line is reserved but not a target.
  CHECK(!out.tap(240, box.status.y + 5));
}

void testCompleteIsSaidOnlyWhenItIs() {
  todoui::ItemRow rows[2];
  rows[0].text = "Buy milk";
  rows[0].complete = true;
  rows[1].text = "Buy coffee";
  rows[1].complete = true;
  rows[1].value = 1;
  todoui::ItemsModel model;
  model.title = "SHOPPING";
  model.rows = rows;
  model.count = 2;
  model.complete = true;

  Rendered done;
  done.items(model);
  CHECK(done.drew("COMPLETE"));
  const todoui::Layout box = todoui::layout(device(), true);
  bool inStatus = false;
  for (const DrawnText& drawn : done.target.texts) {
    if (drawn.text == "COMPLETE") inStatus = sameRect(drawn.rect, box.status);
  }
  CHECK(inStatus);
  // The rows sit where they sit whether or not the line is written.
  const std::vector<fui::Rect> rects = itemRects(done.target, rows, 2);
  CHECK(done.tap(240, rects[0].y + 10).action == todoui::ActionToggleItem);

  // One back to O and the word goes.
  rows[0].complete = false;
  model.complete = false;
  Rendered undone;
  undone.items(model);
  CHECK(!undone.drew("COMPLETE"));
  CHECK(undone.tap(240, rects[0].y + 10).action == todoui::ActionToggleItem);

  // Empty: not complete, and says what it is instead.
  todoui::ItemsModel empty;
  empty.title = "NEW";
  empty.empty = true;
  Rendered blank;
  blank.items(empty);
  CHECK(blank.drew("No items yet."));
  CHECK(!blank.drew("COMPLETE"));
  CHECK(blank.slotsFor(todoui::ActionToggleItem) == 0);
  CHECK(blank.tap(240, box.action.y + 10).action == todoui::ActionAddItem);
}

void testBinModeMarksInsteadOfTicking() {
  todoui::ItemRow rows[3];
  rows[0].text = "Buy milk";
  rows[0].value = 0;
  rows[1].text = "Buy coffee";
  rows[1].complete = true;
  rows[1].value = 1;
  rows[2].text = "Cat food";
  rows[2].value = 2;
  todoui::ItemsModel model;
  model.title = "SHOPPING";
  model.rows = rows;
  model.count = 3;
  const todoui::Layout box = todoui::layout(device(), true);

  // Out of bin mode: the bin is there, ADD ITEM works, no caption, no X marks.
  Rendered plain;
  plain.items(model);
  CHECK(plain.registered(todoui::ActionBin, 0, box.bin));
  CHECK(plain.tap(box.bin.x + 10, box.bin.y + 10).action == todoui::ActionBin);
  CHECK(plain.tap(box.action.x + 10, box.action.y + 10).action == todoui::ActionAddItem);
  CHECK(!plain.drew("TAP ITEMS TO REMOVE"));
  const std::vector<fui::Rect> rects = itemRects(plain.target, rows, 3);
  auto glyphOf = [](const fui::Rect& row) {
    const fui::Rect markBox = todoui::markRect(row);
    return fui::makeRect(static_cast<int16_t>(markBox.x + (markBox.width - 24) / 2),
                         static_cast<int16_t>(markBox.y + (markBox.height - 24) / 2), 24, 24);
  };
  // Only the done row carries a glyph (its tick).
  CHECK(!plain.blitted(glyphOf(rects[0])));
  CHECK(plain.blitted(glyphOf(rects[1])));
  CHECK(!plain.blitted(glyphOf(rects[2])));

  // In bin mode, with the first and the done row marked.
  rows[0].doomed = true;
  rows[1].doomed = true;
  model.binning = true;
  model.complete = true;  // would say COMPLETE, but the bin's caption wins
  Rendered binning;
  binning.items(model);
  CHECK(binning.drew("TAP ITEMS TO REMOVE"));
  CHECK(!binning.drew("COMPLETE"));
  // The bin still answers; ADD ITEM is drawn but takes no tap.
  CHECK(binning.tap(box.bin.x + 10, box.bin.y + 10).action == todoui::ActionBin);
  CHECK(binning.drew("ADD ITEM"));
  CHECK(binning.slotsFor(todoui::ActionAddItem) == 0);
  CHECK(!binning.tap(box.action.x + 10, box.action.y + 10));
  // Rows are where they were, and still route as item taps; the activity
  // reads them as marks while binning.
  const std::vector<fui::Rect> binRects = itemRects(binning.target, rows, 3);
  for (size_t i = 0; i < 3; ++i) CHECK(sameRect(binRects[i], rects[i]));
  CHECK(binning.tap(200, rects[2].y + 10).action == todoui::ActionToggleItem);
  // A marked row shows an X in an OUTLINED box, whatever its own state: the
  // done row's slab and tick give way to it.
  CHECK(binning.blitted(glyphOf(rects[0])));
  CHECK(binning.stroked(todoui::markRect(rects[0])));
  CHECK(binning.blitted(glyphOf(rects[1])));
  CHECK(binning.stroked(todoui::markRect(rects[1])));
  CHECK(!binning.filled(todoui::markRect(rects[1])));
  // The unmarked row is as it was.
  CHECK(!binning.blitted(glyphOf(rects[2])));
  CHECK(binning.stroked(todoui::markRect(rects[2])));

  // A mark is only read in bin mode: the same rows out of it draw no X.
  model.binning = false;
  Rendered after;
  after.items(model);
  CHECK(!after.blitted(glyphOf(rects[0])));
  CHECK(after.filled(todoui::markRect(rects[1])));
}

}  // namespace

int main() {
  testCompletionIsDerivedFromTheItems();
  testTogglingAnItemUpdatesCompletion();
  testMarkAllSetsEveryItem();
  testSortingRespectsTheGroups();
  testOrderIsStableWithinAGroup();
  testDeletingRemovesTheRightList();
  testRemovingItemsTakesExactlyTheMarkedOnes();
  testTheFileRoundTrips();
  testADamagedFileCostsOnlyItsDamagedLines();
  testBlankNamesAreRefused();
  testPagingStopsAtBothEnds();
  testLayoutFitsThePanelUnderTheChrome();
  testRowsAreAsTallAsTheirText();
  testPagesFillGreedily();
  testAListRowOpensAndItsDotsDoNot();
  testALongNameWrapsAndMovesTheRowsBelowIt();
  testAFullPageOfListsFitsTheTable();
  testTheEmptyShelfSaysSo();
  testAnItemRowTogglesWhereverItIsTapped();
  testCompleteIsSaidOnlyWhenItIs();
  testBinModeMarksInsteadOfTicking();

  std::printf("todo: %d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
