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
const char* const kBinCaption = "TAP ITEMS TO REMOVE";

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

// Where a row's text starts: the row's side padding, the box and a gutter,
// the same arrangement the shelf chooser's rows have.
constexpr int16_t kTextInset = toybox::kGutter + kBoxSize + toybox::kGutter;

// Air above and below a row's text. Small, because a single-line row is
// already held open to the theme's row height; this is what a wrapped row
// adds around its lines.
constexpr int16_t kRowPad = 4;

fui::StyleSet invisible() {
  // A hit region and nothing else. A StyleSet left unset would be replaced by
  // the default button look, which is a black slab the size of the control.
  fui::StyleSet styles;
  styles.explicitlySet = true;
  return styles;
}

// The bin while bin mode is on: the slab turned inside out, paper with a heavy
// black edge and a black X, so the one control that changed state is the one
// that looks different from every other pill on the device.
fui::StyleSet binOnStyles() {
  fui::StyleSet styles;
  styles.explicitlySet = true;
  styles.normal.background = fui::Paint::solid(fui::Color::White);
  styles.normal.foreground = fui::Paint::solid(fui::Color::Black);
  styles.normal.border = fui::Paint::solid(fui::Color::Black);
  styles.normal.borderWidth = static_cast<uint8_t>(toybox::kRule);
  styles.selected = styles.normal;
  styles.focused = styles.normal;
  styles.active = styles.normal;
  styles.disabled = styles.normal;
  return styles;
}

// The face a row's text is set in: the theme's body text, which under the
// activity's faces is the reading cut. Named in one place because the height
// of a row is measured in it and the row is drawn in it.
fui::TextStyle labelStyle(const fui::ThemeTokens& tokens, const int maxLines) {
  fui::TextStyle style = tokens.bodyText;
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Left;
  style.maxLines = static_cast<uint8_t>(maxLines);
  return style;
}

// The detail line under a list's name, in the device's voice.
fui::TextStyle detailStyle(const fui::ThemeTokens& tokens) {
  fui::TextStyle style = toybox::buttonText(tokens);
  style.align = fui::TextAlign::Left;
  return style;
}

// The text as it will be drawn, and how many lines that takes. fitLines()
// wraps the way the renderer wraps and cuts with an ellipsis only when the
// lines run out; measureWrappedText() then counts the lines the result really
// uses, so a name that fits on one line costs one.
struct FittedText {
  std::string text;
  int lines = 1;
};

FittedText fit(const fui::DrawTarget& target, const char* text, const int16_t width, const fui::TextStyle& style) {
  FittedText out;
  out.text = toybox::fitLines(target, text, width, style.maxLines, style);
  const int16_t lineHeight = target.lineHeight(style.font);
  if (lineHeight > 0 && !out.text.empty()) {
    const fui::Size wrapped = fui::measureWrappedText(target, out.text.c_str(), style, width);
    out.lines = wrapped.height / lineHeight;
  }
  if (out.lines < 1) out.lines = 1;
  if (out.lines > style.maxLines) out.lines = style.maxLines;
  return out;
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

// A row's frame and its target: the theme's list row, a hairline around paper,
// registered whole. The finger is already on the row, so the whole row
// answers; a box under half a thumb would not.
void rowFrame(toybox::Screen& screen, const fui::Rect& row, const fui::ActionId action, const int16_t value) {
  screen.target().stroke(row, fui::Paint::solid(fui::Color::Black), toybox::kHairline);
  screen.frame().hit(row, action, value);
}

// A row's text, and the detail under it when there is one, as a block centred
// on the row. A single-line row is taller than its text and centres it; a
// wrapped row is exactly its text plus padding and centres it just the same.
void rowText(toybox::Screen& screen, const fui::Rect& row, const FittedText& fitted, const int16_t width,
             const fui::TextStyle& label, const char* detail) {
  const int16_t labelLh = screen.target().lineHeight(label.font);
  const fui::TextStyle small = detailStyle(screen.theme());
  const int16_t detailLh = detail != nullptr ? screen.target().lineHeight(small.font) : 0;
  const int16_t blockH = static_cast<int16_t>(labelLh * fitted.lines + detailLh);
  const int16_t left = static_cast<int16_t>(row.x + kTextInset);
  const int16_t top = static_cast<int16_t>(row.y + (row.height - blockH) / 2);
  fui::TextStyle style = label;
  style.maxLines = static_cast<uint8_t>(fitted.lines);
  screen.target().text(fui::makeRect(left, top, width, static_cast<int16_t>(labelLh * fitted.lines)),
                       fitted.text.c_str(), style);
  if (detail != nullptr) {
    screen.target().text(
        fui::makeRect(left, static_cast<int16_t>(top + labelLh * fitted.lines), width, detailLh), detail, small);
  }
}

// The box on a row. Three states: an outline for open, a slab with a tick for
// done, and -- in bin mode only -- an outline with an X for an item that will
// go when the bin is pressed again. The X sits in an OUTLINED box whatever the
// item's own state, so a ticked item chosen for removal reads as "leaving"
// rather than as "done", and the outline says the choice is not yet acted on.
void mark(toybox::Screen& screen, const fui::Rect& row, const bool complete, const bool doomed = false) {
  const fui::Paint ink = fui::Paint::solid(fui::Color::Black);
  const fui::Rect box = markRect(row);
  const fui::Rect glyph = fui::makeRect(static_cast<int16_t>(box.x + (kBoxSize - kTickSize) / 2),
                                        static_cast<int16_t>(box.y + (kBoxSize - kTickSize) / 2), kTickSize, kTickSize);
  if (doomed) {
    screen.target().stroke(box, ink, kBoxEdge, kBoxRadius);
    screen.target().bitmap(glyph, fui::bitmapFromIcon(icon_todo_x_24), fui::BitmapMode::Contain, ink);
    return;
  }
  if (!complete) {
    screen.target().stroke(box, ink, kBoxEdge, kBoxRadius);
    return;
  }
  screen.target().fill(box, ink, kBoxRadius);
  // Paper on the slab: drawn in ink it would be invisible and nothing would
  // warn.
  screen.target().bitmap(glyph, fui::bitmapFromIcon(icon_tick_24), fui::BitmapMode::Contain,
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
  const int16_t footY = static_cast<int16_t>(device.height - toybox::kMargin - toybox::kPillHeight);
  // The bin is a square the pill's height, at the left, and the pill takes
  // what is left after a gutter. The main screen has no bin and its pill runs
  // the whole width.
  const int16_t binW = withStatus ? toybox::kPillHeight : 0;
  out.bin = fui::makeRect(left, footY, binW, toybox::kPillHeight);
  const int16_t pillX = withStatus ? static_cast<int16_t>(out.bin.right() + toybox::kGutter) : left;
  out.action = fui::makeRect(pillX, footY, static_cast<int16_t>(left + width - pillX), toybox::kPillHeight);
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

int16_t nameWidth(const fui::Rect& rows, const bool pinned) {
  // Measured against a row placed anywhere: only widths are used.
  const fui::Rect row = fui::makeRect(rows.x, rows.y, rows.width, 1);
  const int16_t right = static_cast<int16_t>((pinned ? pinRect(row).x : moreRect(row).x) - toybox::kGutter);
  return static_cast<int16_t>(right - (row.x + kTextInset));
}

int16_t itemWidth(const fui::Rect& rows) { return static_cast<int16_t>(rows.width - kTextInset - toybox::kGutter); }

int16_t rowHeightFor(const fui::DrawTarget& target, const fui::ThemeTokens& tokens, const char* text,
                     const int16_t width, const int maxLines, const bool withDetail) {
  const fui::TextStyle label = labelStyle(tokens, maxLines);
  const FittedText fitted = fit(target, text, width, label);
  const int16_t labelLh = target.lineHeight(label.font);
  const int16_t detailLh = withDetail ? target.lineHeight(detailStyle(tokens).font) : 0;
  const int16_t needed = static_cast<int16_t>(labelLh * fitted.lines + detailLh + kRowPad * 2);
  return needed > tokens.rowHeight ? needed : tokens.rowHeight;
}

std::vector<int> pageStarts(const fui::Rect& rows, const int16_t* heights, const int count) {
  std::vector<int> starts;
  starts.push_back(0);
  int used = 0;
  for (int i = 0; i < count; ++i) {
    const int next = used == 0 ? heights[i] : used + kRowGap + heights[i];
    // A page holds what fits; a row that does not fit starts the next one,
    // unless it is the first on its page, in which case it is drawn clipped
    // rather than never.
    if (next > rows.height && used != 0) {
      starts.push_back(i);
      used = heights[i];
    } else {
      used = next;
    }
  }
  return starts;
}

int pageOf(const std::vector<int>& starts, const int index) {
  int page = 0;
  for (size_t p = 0; p < starts.size(); ++p) {
    if (starts[p] <= index) page = static_cast<int>(p);
  }
  return page;
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
  actionPill(screen, box, kNewList, ActionNewList);

  if (model.count <= 0) {
    emptyState(screen, box.rows, kNoLists);
    return;
  }
  const int count = model.count > kMaxRowsOnPage ? kMaxRowsOnPage : model.count;
  const fui::TextStyle label = labelStyle(screen.theme(), kNameLines);

  int16_t y = box.rows.y;
  for (int i = 0; i < count; ++i) {
    const ListRow& row = model.rows[i];
    const int16_t width = nameWidth(box.rows, row.pinned);
    const int16_t height = rowHeightFor(screen.target(), screen.theme(), row.name, width, kNameLines, true);
    // Stacked from the band's top, gap between; a row the page did not really
    // have room for is not drawn below the band.
    if (y + height > box.rows.bottom()) break;
    const fui::Rect rect = fui::makeRect(box.rows.x, y, box.rows.width, height);
    y = static_cast<int16_t>(rect.bottom() + kRowGap);

    // The row first, so the "..." registered after it wins: route() scans
    // newest-first, and the rest of the row still opens the list.
    rowFrame(screen, rect, ActionOpenList, row.value);
    char detail[toybox::kIntTextChars + 8];
    detailFor(row, detail, sizeof(detail));
    rowText(screen, rect, fit(screen.target(), row.name, width, label), width, label, detail);
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
  // do not move when the last box is ticked or the bin is opened. Bin mode
  // takes the line over: what a tap does now matters more than whether the
  // list was finished.
  if (model.binning || model.complete) {
    fui::TextStyle status = toybox::buttonText(screen.theme());
    screen.target().text(box.status, model.binning ? kBinCaption : kComplete, status);
  }

  // The bin: a black square with an X, or, while bin mode is on, the same
  // square inside out. Pressing it is one action whichever state it is in;
  // the activity knows which.
  fui::ButtonProps bin;
  bin.action = ActionBin;
  bin.icon = fui::bitmapFromIcon(icon_todo_x_32);
  if (model.binning) bin.styles = binOnStyles();
  screen.button(bin, box.bin);

  if (model.binning) {
    // Present but not usable: dithered rather than gone, so the foot keeps its
    // shape. Nothing is added while things are being chosen for removal.
    fui::ButtonProps pill;
    pill.label = kAddItem;
    pill.text = toybox::buttonText(screen.theme());
    pill.styles = toybox::disabledButtonStyles();
    pill.enabled = false;
    screen.button(pill, box.action);
  } else {
    actionPill(screen, box, kAddItem, ActionAddItem);
  }

  if (model.empty) {
    emptyState(screen, box.rows, kNoItems);
    return;
  }
  if (model.count <= 0) return;
  const int count = model.count > kMaxRowsOnPage ? kMaxRowsOnPage : model.count;
  const fui::TextStyle label = labelStyle(screen.theme(), kItemLines);
  const int16_t width = itemWidth(box.rows);

  int16_t y = box.rows.y;
  for (int i = 0; i < count; ++i) {
    const ItemRow& row = model.rows[i];
    const int16_t height = rowHeightFor(screen.target(), screen.theme(), row.text, width, kItemLines, false);
    if (y + height > box.rows.bottom()) break;
    const fui::Rect rect = fui::makeRect(box.rows.x, y, box.rows.width, height);
    y = static_cast<int16_t>(rect.bottom() + kRowGap);

    rowFrame(screen, rect, ActionToggleItem, row.value);
    rowText(screen, rect, fit(screen.target(), row.text, width, label), width, label, nullptr);
    mark(screen, rect, row.complete, model.binning && row.doomed);
  }
}

}  // namespace todoui
