#pragma once

// The two screens: every list, and one list.
//
// Free functions over plain models, FreeInkUI and toybox tokens only. No
// renderer, no activity, no storage, which is what lets host-tests/todo build
// the real screens and route real taps at them. See ToyboxScreen.h for why
// screens are written this way.

#include <FreeInkUI.h>

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
  ActionToggleItem = 4,  // an item row, all of it
  ActionAddItem = 5,     // the pill at the foot of a list
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
extern const char* const kNoItemsYet; // the subtitle of an empty list

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
// the top row on the panel. See shelfui::MenuModel for why a page is passed as
// a short list rather than as the whole list plus an offset.
struct ListsModel {
  const ListRow* rows = nullptr;
  int count = 0;
  int page = 0;
  int pageCount = 1;
};

struct ItemRow {
  const char* text = "";
  bool complete = false;
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
  int page = 0;
  int pageCount = 1;
};

// WHERE EVERYTHING IS, decided once, in absolute panel rows (see
// ToyboxMetrics.h). Both screens are the same shape: the chrome, an optional
// status line under it, a band of rows, and a pill on the margin at the foot.
// The builders draw from these rects and register the same rects as targets,
// and the activity asks rowsPerPage() of the same band, so the three cannot
// disagree about where a row is.
struct Layout {
  fui::Rect status;  // one line under the chrome; zero height on the main screen
  fui::Rect rows;    // where the list component lays its rows
  fui::Rect action;  // the pill
};

Layout layout(const fui::DeviceContext& device, bool withStatus);

// Row heights. A list row carries a name and a count under it, and the
// component sizes a two-line row to its two lines whatever the theme says --
// so the number is stated here and handed to the component, rather than read
// back from the theme and found to be wrong by seven pixels. An item row is
// one line and takes the fork's ordinary row.
constexpr int16_t kListRowHeight = 76;
constexpr int16_t kItemRowHeight = toybox::kRowHeight;
constexpr int16_t kRowGap = 4;

// The most rows either band can hold: 800 panel rows less the chrome, the
// margin and the pill, at the shorter row. Bounds the per-row scratch in the
// builders and the activity; a page longer than this is a bug in the paging,
// and is clipped rather than overrun.
constexpr int kMaxRowsOnPage = 12;

// How many rows fit `rows` at `rowHeight`: what the activity pages by.
int rowsPerPage(const fui::Rect& rows, int16_t rowHeight);

// Row `index` of a band, counting from the band's first row, and the two
// things drawn ON a row that the component knows nothing about. Each is used
// twice, by the builder to draw and to register, so the hit target is where
// the ink is by construction.
fui::Rect rowRect(const fui::Rect& rows, int index, int16_t rowHeight);
// The box at the left: outlined for open, filled with a tick for complete.
fui::Rect markRect(const fui::Rect& row);
// The "..." control at the right of a list row: a thumb wide, the row tall.
fui::Rect moreRect(const fui::Rect& row);
// The pin, just inside the "..." control, on a pinned row.
fui::Rect pinRect(const fui::Rect& row);

void buildLists(toybox::Screen& screen, const ListsModel& model);
void buildItems(toybox::Screen& screen, const ItemsModel& model);

}  // namespace todoui
