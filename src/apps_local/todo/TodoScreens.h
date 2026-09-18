#pragma once

// The two screens: every list, and one list.
//
// Free functions over plain models, FreeInkUI and toybox tokens only. No
// renderer, no activity, no storage, which is what lets host-tests/todo build
// the real screens and route real taps at them. See ToyboxScreen.h for why
// screens are written this way.
//
// The rows are drawn here rather than by the SDK's list component, and the
// reason is wrapping. A name or an item that does not fit one line wraps onto
// the next, so a row is as tall as its text; the component can grow a row too,
// but it then owns where every row is, and the "..." control and the pin this
// screen draws ON a row would have to guess. Here one function says how tall a
// row is, one loop places them, and the same rects are drawn and registered.

#include <FreeInkUI.h>

#include <vector>

#include "../ui/ToyboxScreen.h"

namespace todoui {

namespace fui = freeink::ui;

// Everything a tap on either screen can mean. `value` on the row actions is
// the row's index in the STORE, not its position on the page: the main screen
// shows lists in a derived order, and a screen that reported "the third row"
// would leave the activity converting positions back to lists in a second
// place.
enum : fui::ActionId {
  ActionOpenList = 1,    // the body of a list row
  ActionListMenu = 2,    // the "..." at the right of a list row
  ActionNewList = 3,     // the pill at the foot of the main screen
  ActionToggleItem = 4,  // an item row, all of it; in bin mode, marks it instead
  ActionAddItem = 5,     // the pill at the foot of a list
  // The square at the left of the foot: enters bin mode, and pressed again
  // removes what was marked (or, with nothing marked, just leaves the mode).
  ActionBin = 6,
};

// The words on the screens, named so the tests can ask for them by name.
extern const char* const kTitle;      // the main screen's band
extern const char* const kNewList;    // the pill under the lists
extern const char* const kAddItem;    // the pill under the items
extern const char* const kComplete;   // the status line of a finished list
extern const char* const kNoLists;    // the empty main screen
extern const char* const kNoItems;    // the empty list
extern const char* const kOpenOne;    // "1 OPEN"
extern const char* const kOpenMany;   // "%d OPEN"
extern const char* const kNoItemsYet; // the detail line of an empty list
extern const char* const kBinCaption; // the status line while in bin mode

// One row of the main screen. Strings are borrowed for the length of the build.
struct ListRow {
  const char* name = "";
  int open = 0;          // items still to do
  int total = 0;         // items in the list
  bool pinned = false;
  bool complete = false;
  int16_t value = 0;     // index in the store; see the enum
};

// The current page's rows, and only those: the caller slices, so rows[0] is
// the top row on the panel. The caller paged with pageStarts() over the same
// heights this builder measures, so every row it hands over fits.
struct ListsModel {
  const ListRow* rows = nullptr;
  int count = 0;
  int page = 0;
  int pageCount = 1;
};

struct ItemRow {
  const char* text = "";
  bool complete = false;
  // Marked for removal. Only read in bin mode, where it replaces the box's
  // tick or outline with an X; the item itself is untouched until the bin is
  // pressed again.
  bool doomed = false;
  int16_t value = 0;     // index in the list
};

struct ItemsModel {
  const char* title = "";  // the list's name, as the band shows it
  const ItemRow* rows = nullptr;
  int count = 0;
  // Whether the WHOLE list is complete, not just the rows on this page. The
  // activity derives it from the store, because the page cannot.
  bool complete = false;
  // Whether the whole list has no items. Distinct from count == 0, which is
  // also true of an empty page past the end.
  bool empty = false;
  // Bin mode: the bin button is drawn pressed, the status line says what a tap
  // now does, ADD ITEM is dimmed and disabled, and each row shows its `doomed`
  // mark. The rows themselves neither move nor change size.
  bool binning = false;
  int page = 0;
  int pageCount = 1;
};

// WHERE EVERYTHING IS, decided once, in absolute panel rows (see
// ToyboxMetrics.h). Both screens are the same shape: the chrome, an optional
// status line under it, a band of rows, and a pill on the margin at the foot.
struct Layout {
  fui::Rect status;  // one line under the chrome; zero height on the main screen
  fui::Rect rows;    // where the rows are stacked
  fui::Rect bin;     // the square at the left of the foot; zero width on the main screen
  fui::Rect action;  // the pill, taking the rest of the foot
};

// `withStatus` is the list screen, which is also the screen with the bin.
Layout layout(const fui::DeviceContext& device, bool withStatus);

// How many lines a text may take before it is cut with an ellipsis. Three,
// for both: the keyboard stops at todo::kMaxTextChars, and at the reading
// cut's width a pinned name of that length needs three lines. Anything that
// can be typed therefore wraps whole; the ellipsis is left for a file edited
// by hand.
constexpr int kNameLines = 3;
constexpr int kItemLines = 3;

constexpr int16_t kRowGap = 4;

// The most rows either band can hold: 800 panel rows less the chrome, the
// margin and the pill, at the shortest row. Bounds the per-row scratch in the
// builders and the activity; a page longer than this is a bug in the paging,
// and is clipped rather than overrun.
constexpr int kMaxRowsOnPage = 12;

// The width a row's text may run: from the text inset to the pin or the "..."
// on the main screen, to the row's own padding on a list's.
int16_t nameWidth(const fui::Rect& rows, bool pinned);
int16_t itemWidth(const fui::Rect& rows);

// How tall a row is: its text wrapped to at most `maxLines` in `width`, the
// detail line under it when `withDetail`, and padding, never shorter than the
// theme's row. The activity asks this to page and the builder asks it to draw,
// with the same arguments, so the two cannot disagree.
int16_t rowHeightFor(const fui::DrawTarget& target, const fui::ThemeTokens& tokens, const char* text, int16_t width,
                     int maxLines, bool withDetail);

// The first row of every page, filling each page greedily with rows of the
// given `heights` separated by kRowGap. Always at least one page, so
// `starts.size()` is the page count; a row taller than the band gets a page
// of its own rather than being skipped.
std::vector<int> pageStarts(const fui::Rect& rows, const int16_t* heights, int count);
// The page holding row `index`.
int pageOf(const std::vector<int>& starts, int index);

// The things drawn ON a row. Each is used twice, by the builder to draw and to
// register, so the hit target is where the ink is by construction.
// The box at the left: outlined for open, filled with a tick for complete.
fui::Rect markRect(const fui::Rect& row);
// The "..." control at the right of a list row: a thumb wide, the row tall.
fui::Rect moreRect(const fui::Rect& row);
// The pin, just inside the "..." control, on a pinned row.
fui::Rect pinRect(const fui::Rect& row);

void buildLists(toybox::Screen& screen, const ListsModel& model);
void buildItems(toybox::Screen& screen, const ItemsModel& model);

}  // namespace todoui
