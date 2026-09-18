#include "TodoScreens.h"

#include <FreeInkUIIcon.h>

#include <cstdio>
#include <string>

#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxIcons.h"
#include "TodoIcons.h"

namespace todoui {

const char* const kTitle = "TO DO";
const char* const kNewList = "NEW LIST";
const char* const kAddItem = "ADD ITEM";
const char* const kComplete = "COMPLETE";
const char* const kNoLists = "No lists yet.";
const char* const kNoItems = "No items yet.";
const char* const kOpenOne = "1 OPEN";
const char* const kOpenMany = "%d OPEN";
const char* const kNoItemsYet = "NO ITEMS";

namespace {

// The box on a row, the shelf chooser's exactly: a filled slab with the tick
// knocked out of it, or a hairline outline. That is this fork's existing
// language for "this one is done", so a to-do list needs no language of its
// own. The tick is Lucide's because a hand-drawn tick is what goes wrong.
constexpr int16_t kBoxSize = toybox::kIconSize;
constexpr int16_t kTickSize = 24;
constexpr uint8_t kBoxRadius = 6;
constexpr int16_t kBoxEdge = 2;

// The "..." control: a thumb wide, so it can be hit without hitting the row
// under it, and no wider, so the name keeps the rest.
constexpr int16_t kMoreWidth = 64;
constexpr int16_t kPinSize = 24;

// The status line is one line of the button cut: it is the device speaking
// rather than the list's content.
constexpr int16_t kStatusHeight = toybox::kButtonCut.lineHeight;

// Where the list component starts a row's text: its own side padding, the box
// and a gutter, the same arrangement the shelf chooser's rows have.
constexpr int16_t kTextInset = toybox::kGutter + kBoxSize + toybox::kGutter;

fui::StyleSet invisible() {
  // A hit region and nothing else. A StyleSet left unset would be replaced by
  // the default button look, which is a black slab the size of the control.
  fui::StyleSet styles;
  styles.explicitlySet = true;
  return styles;
}

// The header band, with the page counter at its right when there is more than
// one page. Placed by hand with the small cut's ink centred in the visible
// band, the way the shelf places its own counter, because the component's
// rightLabel sits on the display cut's line box and reads as dropped.
void chrome(toybox::Screen& screen, const char* title, const int page, const int pageCount) {
  char counter[toybox::kSlashCounterChars] = {};
  fui::TextStyle style;
  style.font = toybox::kSmallFont;
  style.align = fui::TextAlign::Right;
  style.color = fui::Color::White;

  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  if (pageCount > 1) {
    std::snprintf(counter, sizeof(counter), "%d/%d", page + 1, pageCount);
    const int16_t width = screen.target().measureText(style.font, counter, style).width;
    header.rightReserve = static_cast<int16_t>(width + toybox::kGutter);
  }
  toybox::headerBand(screen, header);

  if (pageCount > 1) {
    const int16_t width = screen.device().screen().width;
    const fui::Rect box =
        fui::makeRect(0, toybox::bandCenterY(screen, toybox::kButtonCut.inkHeight),
                      static_cast<int16_t>(width - toybox::kMargin), toybox::kButtonCut.inkHeight);
    screen.target().text(toybox::inkCentred(box, toybox::kButtonCut), counter, style);
  }
}

// The content rect becomes exactly the rows band, so the list component lays
// its rows where layout() says they are. One rect, two readers.
void contentToRows(toybox::Screen& screen, const Layout& box) {
  const fui::Rect body = screen.body();
  screen.insetContent(fui::Insets{static_cast<int16_t>(box.rows.y - body.y),
                                  static_cast<int16_t>(body.right() - box.rows.right()),
                                  static_cast<int16_t>(body.bottom() - box.rows.bottom()),
                                  static_cast<int16_t>(box.rows.x - body.x)});
}

// The pill on the margin. Jersey for the label whatever the body slot carries,
// because a button is the device speaking.
void actionPill(toybox::Screen& screen, const Layout& box, const char* label, const fui::ActionId action) {
  fui::ButtonProps pill;
  pill.label = label;
  pill.action = action;
  pill.text = toybox::buttonText(screen.theme());
  screen.button(pill, box.action);
}

// One line of the reading cut, centred in the band the rows would have taken.
void emptyState(toybox::Screen& screen, const fui::Rect& rows, const char* words) {
  fui::TextStyle style = screen.theme().bodyText;
  // Off the band, so ink: the token colour is only right on black.
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Center;
  const int16_t lineHeight = screen.target().lineHeight(style.font);
  screen.target().text(
      fui::makeRect(rows.x, static_cast<int16_t>(rows.y + (rows.height - lineHeight) / 2), rows.width, lineHeight),
      words, style);
}

void mark(toybox::Screen& screen, const fui::Rect& row, const bool complete) {
  const fui::Paint ink = fui::Paint::solid(fui::Color::Black);
  const fui::Rect box = markRect(row);
  if (!complete) {
    screen.target().stroke(box, ink, kBoxEdge, kBoxRadius);
    return;
  }
  screen.target().fill(box, ink, kBoxRadius);
  // Paper on the slab: drawn in ink it would be invisible and nothing would
  // warn.
  screen.target().bitmap(fui::makeRect(static_cast<int16_t>(box.x + (kBoxSize - kTickSize) / 2),
                                       static_cast<int16_t>(box.y + (kBoxSize - kTickSize) / 2), kTickSize, kTickSize),
                         fui::bitmapFromIcon(icon_tick_24), fui::BitmapMode::Contain,
                         fui::Paint::solid(fui::Color::White));
}

// The count under a list's name, in the device's voice.
void detailFor(const ListRow& row, char* out, const size_t size) {
  if (row.total == 0) {
    std::snprintf(out, size, "%s", kNoItemsYet);
  } else if (row.complete) {
    std::snprintf(out, size, "%s", kComplete);
  } else if (row.open == 1) {
    std::snprintf(out, size, "%s", kOpenOne);
  } else {
    std::snprintf(out, size, kOpenMany, row.open);
  }
}

}  // namespace

Layout layout(const fui::DeviceContext& device, const bool withStatus) {
  const int16_t left = toybox::kMargin;
  const int16_t width = static_cast<int16_t>(device.width - toybox::kMargin * 2);

  Layout out;
  out.action = fui::makeRect(left, static_cast<int16_t>(device.height - toybox::kMargin - toybox::kPillHeight), width,
                             toybox::kPillHeight);
  // Taken from toybox::kBodyTop rather than re-summed, so it cannot drift from
  // what headerBand() reserves.
  int16_t top = toybox::kBodyTop;
  out.status = fui::makeRect(left, top, width, withStatus ? kStatusHeight : 0);
  if (withStatus) top = static_cast<int16_t>(out.status.bottom() + toybox::kGutter);
  // Two gutters above the pill: a filled slab directly under a row reads as
  // part of the list.
  const int16_t bottom = static_cast<int16_t>(out.action.y - toybox::kGutter * 2);
  out.rows = fui::makeRect(left, top, width, static_cast<int16_t>(bottom - top));
  return out;
}

int rowsPerPage(const fui::Rect& rows, const int16_t rowHeight) {
  const int fit = fui::listVisibleRows(rows, rowHeight, kRowGap);
  if (fit < 1) return 1;
  return fit > kMaxRowsOnPage ? kMaxRowsOnPage : fit;
}

fui::Rect rowRect(const fui::Rect& rows, const int index, const int16_t rowHeight) {
  return fui::makeRect(rows.x, static_cast<int16_t>(rows.y + index * (rowHeight + kRowGap)), rows.width, rowHeight);
}

fui::Rect markRect(const fui::Rect& row) {
  return fui::makeRect(static_cast<int16_t>(row.x + toybox::kGutter),
                       static_cast<int16_t>(row.y + (row.height - kBoxSize) / 2), kBoxSize, kBoxSize);
}

fui::Rect moreRect(const fui::Rect& row) {
  return fui::makeRect(static_cast<int16_t>(row.right() - kMoreWidth), row.y, kMoreWidth, row.height);
}

fui::Rect pinRect(const fui::Rect& row) {
  const fui::Rect more = moreRect(row);
  return fui::makeRect(static_cast<int16_t>(more.x - kPinSize), static_cast<int16_t>(row.y + (row.height - kPinSize) / 2),
                       kPinSize, kPinSize);
}

void buildLists(toybox::Screen& screen, const ListsModel& model) {
  chrome(screen, kTitle, model.page, model.pageCount);
  const Layout box = layout(screen.device(), false);
  contentToRows(screen, box);
  actionPill(screen, box, kNewList, ActionNewList);

  if (model.count <= 0) {
    emptyState(screen, box.rows, kNoLists);
    return;
  }
  const int count = model.count > kMaxRowsOnPage ? kMaxRowsOnPage : model.count;

  // The name is fitted to the room the row really gives it -- short of the pin
  // and the "..." -- before the component sees it, so the component never has
  // to cut it, and never cuts it with a glyph the face does not carry.
  fui::TextStyle labelStyle = screen.theme().bodyText;
  fui::TextStyle subtitleStyle = toybox::buttonText(screen.theme());
  subtitleStyle.align = fui::TextAlign::Left;

  std::string names[kMaxRowsOnPage];
  char details[kMaxRowsOnPage][toybox::kIntTextChars + 8];
  fui::ListItem items[kMaxRowsOnPage];
  for (int i = 0; i < count; ++i) {
    const ListRow& row = model.rows[i];
    const fui::Rect rect = rowRect(box.rows, i, kListRowHeight);
    const int16_t textLeft = static_cast<int16_t>(rect.x + kTextInset);
    const int16_t textRight =
        static_cast<int16_t>((row.pinned ? pinRect(rect).x : moreRect(rect).x) - toybox::kGutter);
    names[i] = toybox::fitLines(screen.target(), row.name, static_cast<int16_t>(textRight - textLeft), 1, labelStyle);
    detailFor(row, details[i], sizeof(details[i]));
    items[i].label = names[i].c_str();
    items[i].subtitle = details[i];
    items[i].actionValue = row.value;
  }

  fui::ListProps list;
  list.items = items;
  list.count = static_cast<uint16_t>(count);
  list.topIndex = 0;
  // Touch only: nothing moves a cursor here, so no row is ever the marked one.
  list.selectedIndex = -1;
  list.action = ActionOpenList;
  list.labelText = labelStyle;
  list.subtitleText = subtitleStyle;
  // Stated, not inherited: the component sizes a two-line row to its lines,
  // and rowRect() above has to agree with it. See kListRowHeight.
  list.rowHeight = kListRowHeight;
  list.rowGap = kRowGap;
  list.sidePadding = kTextInset;
  screen.list(list);

  // What the component does not draw, on the rows it did. Registered AFTER the
  // list, because route() scans newest-first: the "..." wins over the row
  // under it, and the rest of the row still opens the list.
  for (int i = 0; i < count; ++i) {
    const ListRow& row = model.rows[i];
    const fui::Rect rect = rowRect(box.rows, i, kListRowHeight);
    mark(screen, rect, row.complete);

    if (row.pinned) {
      screen.target().bitmap(pinRect(rect), fui::bitmapFromIcon(icon_todo_pin_24), fui::BitmapMode::Contain,
                             fui::Paint::solid(fui::Color::Black));
    }

    const fui::Rect more = moreRect(rect);
    screen.target().bitmap(fui::makeRect(static_cast<int16_t>(more.x + (more.width - toybox::kIconSize) / 2),
                                         static_cast<int16_t>(more.y + (more.height - toybox::kIconSize) / 2),
                                         toybox::kIconSize, toybox::kIconSize),
                           fui::bitmapFromIcon(icon_todo_more_32), fui::BitmapMode::Contain,
                           fui::Paint::solid(fui::Color::Black));
    fui::ButtonProps menu;
    menu.action = ActionListMenu;
    menu.value = row.value;
    menu.styles = invisible();
    menu.minTouchSize = 0;
    screen.button(menu, more);
  }
}

void buildItems(toybox::Screen& screen, const ItemsModel& model) {
  chrome(screen, model.title, model.page, model.pageCount);
  const Layout box = layout(screen.device(), true);

  // The status line is always reserved and only sometimes written, so the rows
  // do not move when the last box is ticked.
  if (model.complete) {
    fui::TextStyle status = toybox::buttonText(screen.theme());
    screen.target().text(box.status, kComplete, status);
  }

  contentToRows(screen, box);
  actionPill(screen, box, kAddItem, ActionAddItem);

  if (model.empty) {
    emptyState(screen, box.rows, kNoItems);
    return;
  }
  if (model.count <= 0) return;
  const int count = model.count > kMaxRowsOnPage ? kMaxRowsOnPage : model.count;

  fui::TextStyle labelStyle = screen.theme().bodyText;
  std::string texts[kMaxRowsOnPage];
  fui::ListItem items[kMaxRowsOnPage];
  for (int i = 0; i < count; ++i) {
    const ItemRow& row = model.rows[i];
    const fui::Rect rect = rowRect(box.rows, i, kItemRowHeight);
    const int16_t width = static_cast<int16_t>(rect.width - kTextInset - toybox::kGutter);
    texts[i] = toybox::fitLines(screen.target(), row.text, width, 1, labelStyle);
    items[i].label = texts[i].c_str();
    items[i].actionValue = row.value;
  }

  fui::ListProps list;
  list.items = items;
  list.count = static_cast<uint16_t>(count);
  list.topIndex = 0;
  list.selectedIndex = -1;
  // The whole row toggles. One target per row rather than a box and a label,
  // because the finger is already on the row and a box is under half a thumb.
  list.action = ActionToggleItem;
  list.labelText = labelStyle;
  list.rowHeight = kItemRowHeight;
  list.rowGap = kRowGap;
  list.sidePadding = kTextInset;
  screen.list(list);

  for (int i = 0; i < count; ++i) {
    mark(screen, rowRect(box.rows, i, kItemRowHeight), model.rows[i].complete);
  }
}

}  // namespace todoui
