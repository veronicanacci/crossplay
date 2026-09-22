#include "LibraryScreens.h"

#include <FreeInkUIIcon.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../ui/ToyboxFormat.h"
#include "LibraryArt.h"
#include "LibraryIcons.h"

namespace libraryui {

const char* const kTitle = "MY LIBRARY";
const char* const kNoBooks = "No books here.";
const char* const kNoGroups = "Nothing here yet.";
const char* const kInfo = "INFO";
const char* const kSummary = "SUMMARY";

namespace {

constexpr int16_t kRowPad = 6;
// Where a row's text starts: the row's own side padding.
constexpr int16_t kTextInset = toybox::kGutter;
// One line of the reading cut under the band, saying which books these are.
constexpr int16_t kCaptionHeight = 40;

fui::StyleSet invisible() {
  fui::StyleSet styles;
  styles.explicitlySet = true;
  return styles;
}

// The bold reading cut, which readingAddressFaces() binds to the small slot.
fui::TextStyle boldStyle(const fui::ThemeTokens& tokens, const int maxLines) {
  fui::TextStyle style = tokens.bodyText;
  style.font = fui::FONT_SLOT_SMALL;
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Left;
  style.maxLines = static_cast<uint8_t>(maxLines);
  return style;
}

// The reading cut, in the body slot.
fui::TextStyle proseStyle(const fui::ThemeTokens& tokens, const int maxLines = 1) {
  fui::TextStyle style = tokens.bodyText;
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Left;
  style.maxLines = static_cast<uint8_t>(maxLines);
  return style;
}

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
// one page. The counter is set in the body slot, whatever cut that carries, and
// centred in the visible band by hand the way the shelf places its own.
void chrome(toybox::Screen& screen, const char* title, const int page, const int pageCount,
            fui::HeaderProps header = fui::HeaderProps{}) {
  char counter[toybox::kSlashCounterChars] = {};
  fui::TextStyle style;
  style.font = fui::FONT_SLOT_BODY;
  style.align = fui::TextAlign::Right;
  style.color = fui::Color::White;

  header.title = title;
  header.borderEdges = fui::EdgesNone;
  if (pageCount > 1) {
    std::snprintf(counter, sizeof(counter), "%d/%d", page + 1, pageCount);
    const int16_t width = screen.target().measureText(style.font, counter, style).width;
    header.rightReserve = static_cast<int16_t>(header.rightReserve + width + toybox::kGutter);
  }
  toybox::headerBand(screen, header);

  if (pageCount > 1) {
    const int16_t lineHeight = screen.target().lineHeight(style.font);
    int16_t right = static_cast<int16_t>(screen.device().screen().width - toybox::kMargin);
    // Past the trailing chip when the band carries one.
    if (header.trailingAction != fui::NO_ACTION) {
      right = static_cast<int16_t>(right - (screen.theme().headerHeight - 8) - toybox::kGutter);
    }
    const fui::Rect box = fui::makeRect(0, toybox::bandCenterY(screen, lineHeight), right, lineHeight);
    screen.target().text(box, counter, style);
  }
}

// One line of the reading cut, centred in the band the rows would have taken.
void emptyState(toybox::Screen& screen, const fui::Rect& rows, const char* words) {
  fui::TextStyle style = proseStyle(screen.theme(), 3);
  style.align = fui::TextAlign::Center;
  const int16_t height = fui::measureWrappedText(screen.target(), words, style, rows.width).height;
  screen.target().text(
      fui::makeRect(rows.x, static_cast<int16_t>(rows.y + (rows.height - height) / 2), rows.width, height), words,
      style);
}

}  // namespace

// ---- menus ------------------------------------------------------------------

fui::Rect menuBand(const fui::DeviceContext& device) {
  return fui::makeRect(toybox::kMargin, toybox::kBodyTop, static_cast<int16_t>(device.width - 2 * toybox::kMargin),
                       static_cast<int16_t>(device.height - toybox::kMargin - toybox::kBodyTop));
}

int menuRowsPerPage(const fui::DeviceContext& device, const fui::ThemeTokens& tokens) {
  const int rows = fui::listVisibleRows(menuBand(device), tokens.rowHeight, tokens.listRowGap);
  return rows > 0 ? rows : 1;
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  chrome(screen, model.title, model.page, model.pageCount);
  screen.insetContent(fui::Insets{toybox::kBodyGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  if (model.count <= 0) {
    if (model.empty != nullptr) emptyState(screen, menuBand(screen.device()), model.empty);
    return;
  }

  fui::ListProps list;
  list.items = model.items;
  list.count = static_cast<uint16_t>(model.count);
  list.topIndex = 0;
  // No cursor: navigation here is touch, and the side keys page.
  list.selectedIndex = -1;
  list.action = ActionRow;
  if (model.reading) {
    // The bold reading cut for the names, the reading cut for the count beside
    // them. Both explicit, or Screen substitutes the theme's Jersey.
    list.labelText = boldStyle(screen.theme(), 1);
    list.valueText = proseStyle(screen.theme(), 1);
    list.valueText.align = fui::TextAlign::Right;
  }
  screen.list(list);
}

// ---- book lists -------------------------------------------------------------

fui::Rect bookRows(const fui::DeviceContext& device) {
  const int16_t top = static_cast<int16_t>(toybox::kBodyTop + kCaptionHeight + toybox::kGutter);
  return fui::makeRect(toybox::kMargin, top, static_cast<int16_t>(device.width - 2 * toybox::kMargin),
                       static_cast<int16_t>(device.height - toybox::kMargin - top));
}

int16_t bookTextWidth(const fui::Rect& rows) { return static_cast<int16_t>(rows.width - 2 * kTextInset); }

int16_t bookRowHeight(const fui::DrawTarget& target, const fui::ThemeTokens& tokens, const char* title,
                      const int16_t width) {
  const fui::TextStyle bold = boldStyle(tokens, kTitleLines);
  const FittedText fitted = fit(target, title, width, bold);
  const int16_t titleLh = target.lineHeight(bold.font);
  const int16_t authorLh = target.lineHeight(proseStyle(tokens).font);
  const int16_t needed = static_cast<int16_t>(titleLh * fitted.lines + authorLh + kRowPad * 2);
  return needed > tokens.rowHeight ? needed : tokens.rowHeight;
}

std::vector<int> pageStarts(const fui::Rect& rows, const int16_t* heights, const int count) {
  std::vector<int> starts;
  starts.push_back(0);
  int used = 0;
  for (int i = 0; i < count; ++i) {
    const int next = used == 0 ? heights[i] : used + kRowGap + heights[i];
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

void buildBookList(toybox::Screen& screen, const BookListModel& model) {
  chrome(screen, model.title, model.page, model.pageCount);

  // The caption: which books these are, in the reading cut, under the band.
  const fui::Rect caption =
      fui::makeRect(toybox::kMargin, toybox::kBodyTop,
                    static_cast<int16_t>(screen.device().width - 2 * toybox::kMargin), kCaptionHeight);
  screen.target().text(caption, model.caption, proseStyle(screen.theme()));

  const fui::Rect rows = bookRows(screen.device());
  if (model.empty) {
    emptyState(screen, rows, model.emptyText != nullptr ? model.emptyText : kNoBooks);
    return;
  }
  const int count = model.count > kMaxRowsOnPage ? kMaxRowsOnPage : model.count;
  const fui::TextStyle bold = boldStyle(screen.theme(), kTitleLines);
  const fui::TextStyle prose = proseStyle(screen.theme());
  const int16_t width = bookTextWidth(rows);
  const int16_t titleLh = screen.target().lineHeight(bold.font);
  const int16_t authorLh = screen.target().lineHeight(prose.font);

  int16_t y = rows.y;
  for (int i = 0; i < count; ++i) {
    const BookRow& row = model.rows[i];
    const int16_t height = bookRowHeight(screen.target(), screen.theme(), row.title, width);
    if (y + height > rows.bottom()) break;
    const fui::Rect rect = fui::makeRect(rows.x, y, rows.width, height);
    y = static_cast<int16_t>(rect.bottom() + kRowGap);

    // The theme's list row, a hairline around paper, registered whole.
    screen.target().stroke(rect, fui::Paint::solid(fui::Color::Black), toybox::kHairline);
    screen.frame().hit(rect, ActionOpenBook, row.value);

    const FittedText fitted = fit(screen.target(), row.title, width, bold);
    const int16_t blockH = static_cast<int16_t>(titleLh * fitted.lines + authorLh);
    const int16_t left = static_cast<int16_t>(rect.x + kTextInset);
    const int16_t top = static_cast<int16_t>(rect.y + (rect.height - blockH) / 2);
    fui::TextStyle titleStyle = bold;
    titleStyle.maxLines = static_cast<uint8_t>(fitted.lines);
    screen.target().text(fui::makeRect(left, top, width, static_cast<int16_t>(titleLh * fitted.lines)),
                         fitted.text.c_str(), titleStyle);
    screen.target().text(fui::makeRect(left, static_cast<int16_t>(top + titleLh * fitted.lines), width, authorLh),
                         row.author, prose);
  }
}

// ---- one book -----------------------------------------------------------------

namespace {

constexpr int16_t kHeartBox = toybox::kIconSize + 12;
// How many lines a field's value may take before it is cut.
constexpr int kFieldLines = 3;
constexpr int16_t kHeartGlyph = 24;
constexpr uint8_t kHeartRadius = 6;

// A label in the bold cut and its value in the reading cut on one line. Returns
// the line's height, or 0 when there was nothing to say.
int16_t field(toybox::Screen& screen, const int16_t x, const int16_t y, const int16_t width, const char* label,
              const char* value) {
  if (value == nullptr || *value == '\0') return 0;
  const fui::TextStyle bold = boldStyle(screen.theme(), 1);
  const fui::TextStyle prose = proseStyle(screen.theme(), kFieldLines);
  const int16_t lh = screen.target().lineHeight(bold.font);
  const int16_t proseLh = screen.target().lineHeight(prose.font);
  const int16_t labelW = static_cast<int16_t>(screen.target().measureText(bold.font, label, bold).width);
  screen.target().text(fui::makeRect(x, y, labelW, lh), label, bold);
  const int16_t valueX = static_cast<int16_t>(x + labelW + toybox::kGutter);
  const int16_t valueW = static_cast<int16_t>(x + width - valueX);
  if (valueW <= 0) return lh;
  // A long value wraps in the column beside its label rather than being cut:
  // a series name is the whole point of the line, and one ending in an
  // ellipsis says nothing. The first line sits on the label's line box; the
  // rest follow at the reading cut's own pitch.
  const FittedText fitted = fit(screen.target(), value, valueW, prose);
  fui::TextStyle style = prose;
  style.maxLines = static_cast<uint8_t>(fitted.lines);
  const int16_t valueH = static_cast<int16_t>(fitted.lines == 1 ? lh : lh + proseLh * (fitted.lines - 1));
  screen.target().text(fui::makeRect(valueX, y, valueW, valueH), fitted.text.c_str(), style);
  return valueH;
}

void heart(toybox::Screen& screen, const bool favourite) {
  const fui::Rect box = heartRect(screen.device());
  const fui::Rect glyph =
      fui::makeRect(static_cast<int16_t>(box.x + (box.width - kHeartGlyph) / 2),
                    static_cast<int16_t>(box.y + (box.height - kHeartGlyph) / 2), kHeartGlyph, kHeartGlyph);
  if (favourite) {
    // The fork's language for "this one is on": a filled slab with the mark
    // knocked out of it in paper.
    screen.target().fill(box, fui::Paint::solid(fui::Color::Black), kHeartRadius);
    screen.target().bitmap(glyph, fui::bitmapFromIcon(icon_mylibrary_heart_24), fui::BitmapMode::Contain,
                           fui::Paint::solid(fui::Color::White));
  } else {
    screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), toybox::kHairline, kHeartRadius);
    screen.target().bitmap(glyph, fui::bitmapFromIcon(icon_mylibrary_heart_24), fui::BitmapMode::Contain,
                           fui::Paint::solid(fui::Color::Black));
  }
  fui::ButtonProps toggle;
  toggle.action = ActionHeart;
  toggle.styles = invisible();
  toggle.minTouchSize = 0;
  screen.button(toggle, box);
}

}  // namespace

fui::Rect heartRect(const fui::DeviceContext& device) {
  return fui::makeRect(static_cast<int16_t>(device.width - toybox::kMargin - kHeartBox), toybox::kBodyTop, kHeartBox,
                       kHeartBox);
}

DetailPaging buildDetail(toybox::Screen& screen, const DetailModel& model) {
  DetailPaging paging;

  // The "..." in the band: a paper chip on the black, the way DONE sits on a
  // folder's band, so the one control up there is the one that looks different.
  fui::HeaderProps header;
  header.trailingIcon = fui::bitmapFromIcon(icon_mylibrary_more_24);
  header.trailingAction = ActionMore;
  header.trailingStyles = toybox::bandFilledStyles();
  header.trailingRadius = toybox::kPillRadius / 2;

  // The summary's paging is only known once the block above it is laid out, so
  // the band is drawn without a counter and the counter added after.
  const int16_t x = toybox::kMargin;
  const int16_t width = static_cast<int16_t>(screen.device().width - 2 * toybox::kMargin);
  const fui::Rect heartBox = heartRect(screen.device());
  const int16_t titleW = static_cast<int16_t>(heartBox.x - toybox::kGutter - x);

  const fui::TextStyle bold = boldStyle(screen.theme(), 3);
  const fui::TextStyle prose = proseStyle(screen.theme());
  const int16_t boldLh = screen.target().lineHeight(bold.font);
  const int16_t proseLh = screen.target().lineHeight(prose.font);
  const FittedText title = fit(screen.target(), model.title, titleW, bold);

  struct Line {
    const char* label;
    const char* value;
  };
  char personal[64] = {};
  if (model.showRead) {
    std::snprintf(personal, sizeof(personal), "%s, %s", model.readState, model.rating);
  }
  const Line fields[] = {
      {"ISBN", model.isbn},         {"Publisher", model.publisher}, {"Year", model.year},
      {"Genre", model.genre},       {"Series", model.series},       {"Pages", model.pages},
      {"Language", model.language}, {"Tags", model.tags},           {"Notes", model.notes},
  };

  chrome(screen, model.band, 0, 1, header);

  int16_t y = toybox::kBodyTop;
  fui::TextStyle titleStyle = bold;
  titleStyle.maxLines = static_cast<uint8_t>(title.lines);
  const int16_t titleH = static_cast<int16_t>(boldLh * title.lines);
  screen.target().text(fui::makeRect(x, y, titleW, titleH), title.text.c_str(), titleStyle);
  heart(screen, model.favourite);
  y = static_cast<int16_t>(y + (titleH > heartBox.height ? titleH : heartBox.height));

  if (model.author != nullptr && *model.author != '\0') {
    screen.target().text(fui::makeRect(x, y, titleW, proseLh),
                         toybox::fitLines(screen.target(), model.author, titleW, 1, prose).c_str(), prose);
    y = static_cast<int16_t>(y + proseLh);
  }
  if (model.showRead) {
    screen.target().text(fui::makeRect(x, y, width, proseLh), personal, prose);
    y = static_cast<int16_t>(y + proseLh);
  }

  y = static_cast<int16_t>(y + toybox::kGutter);
  screen.target().fill(fui::makeRect(x, y, width, toybox::kHairline), fui::Paint::solid(fui::Color::Black));
  y = static_cast<int16_t>(y + toybox::kHairline + toybox::kGutter);

  // The foot: an arrow at each end, black slabs like every other button on the
  // device, and the name of the section between them. Two sections, so both
  // arrows go to the other one; two rather than one because a single control
  // that toggles reads as a state, and a pair reads as a place you can leave.
  const int16_t footY = static_cast<int16_t>(screen.device().height - toybox::kMargin - toybox::kPillHeight);
  const fui::Rect prevRect = fui::makeRect(x, footY, toybox::kPillHeight, toybox::kPillHeight);
  const fui::Rect nextRect = fui::makeRect(static_cast<int16_t>(x + width - toybox::kPillHeight), footY,
                                           toybox::kPillHeight, toybox::kPillHeight);
  fui::ButtonProps prev;
  prev.action = ActionSwitchSection;
  prev.value = -1;
  prev.icon = fui::bitmapFromIcon(icon_mylibrary_prev_24);
  screen.button(prev, prevRect);
  fui::ButtonProps next;
  next.action = ActionSwitchSection;
  next.value = 1;
  next.icon = fui::bitmapFromIcon(icon_mylibrary_next_24);
  screen.button(next, nextRect);
  fui::TextStyle sectionStyle = boldStyle(screen.theme(), 1);
  sectionStyle.align = fui::TextAlign::Center;
  const int16_t labelX = static_cast<int16_t>(prevRect.right() + toybox::kGutter);
  screen.target().text(
      fui::makeRect(labelX, footY, static_cast<int16_t>(nextRect.x - toybox::kGutter - labelX), toybox::kPillHeight),
      model.showPlot ? kSummary : kInfo, sectionStyle);

  // Two gutters above the foot: a filled slab directly under a line of text
  // reads as part of it.
  const int16_t bottom = static_cast<int16_t>(footY - toybox::kGutter * 2);
  const fui::Rect section = fui::makeRect(x, y, width, static_cast<int16_t>(bottom > y ? bottom - y : 0));
  if (section.height <= 0) return paging;

  if (!model.showPlot) {
    // INFO: the fields that have a value, one a line, stopping at the foot.
    for (const Line& line : fields) {
      const int16_t lh = screen.target().lineHeight(bold.font);
      if (y + lh > section.bottom()) break;
      y = static_cast<int16_t>(y + field(screen, x, y, width, line.label, line.value));
    }
    return paging;
  }

  const fui::Rect plotRect = section;
  if (model.plot == nullptr || *model.plot == '\0') {
    // No summary: the shrug, at twice its pixel size so it reads at 220ppi, in
    // the middle of the room.
    const int16_t w = library::kShrugWidth * 2;
    const int16_t h = library::kShrugHeight * 2;
    const fui::Rect where = fui::makeRect(static_cast<int16_t>(plotRect.x + (plotRect.width - w) / 2),
                                          static_cast<int16_t>(plotRect.y + (plotRect.height - h) / 2), w, h);
    screen.target().bitmap(where, fui::bitmapFromIcon(library::shrugIcon()), fui::BitmapMode::Contain,
                           fui::Paint::solid(fui::Color::Black));
    return paging;
  }

  paging.visibleLines = fui::textAreaVisibleLines(plotRect, proseLh);
  paging.totalLines =
      static_cast<int>(fui::textAreaMeasure(screen.target(), plotRect.width, model.plot, prose, 0).lineCount);
  if (paging.visibleLines > 0) paging.pageCount = (paging.totalLines + paging.visibleLines - 1) / paging.visibleLines;
  if (paging.pageCount < 1) paging.pageCount = 1;
  paging.page = model.plotPage < 0 ? 0 : (model.plotPage >= paging.pageCount ? paging.pageCount - 1 : model.plotPage);

  fui::TextAreaProps area;
  area.text = model.plot;
  area.style = prose;
  area.showCaret = false;
  area.topLine = static_cast<uint32_t>(paging.page * paging.visibleLines);
  fui::textArea(screen.frame(), plotRect, area);

  // The page counter, now that the pages are known. Drawn over the band the
  // way chrome() would have, past the "..." chip.
  if (paging.pageCount > 1) {
    char counter[toybox::kSlashCounterChars] = {};
    std::snprintf(counter, sizeof(counter), "%d/%d", paging.page + 1, paging.pageCount);
    fui::TextStyle style;
    style.font = fui::FONT_SLOT_BODY;
    style.align = fui::TextAlign::Right;
    style.color = fui::Color::White;
    const int16_t right = static_cast<int16_t>(screen.device().screen().width - toybox::kMargin -
                                               (screen.theme().headerHeight - 8) - toybox::kGutter);
    screen.target().text(fui::makeRect(0, toybox::bandCenterY(screen, proseLh), right, proseLh), counter, style);
  }
  return paging;
}

// ---- reports ------------------------------------------------------------------

void buildReport(toybox::Screen& screen, const ReportModel& model) {
  chrome(screen, model.title, 0, 1);
  const int16_t x = toybox::kMargin;
  const int16_t width = static_cast<int16_t>(screen.device().width - 2 * toybox::kMargin);
  int16_t bottom = static_cast<int16_t>(screen.device().height - toybox::kMargin);
  if (model.action != nullptr) {
    // The pill on the margin, the shape every other foot action on the device
    // has; Jersey for the label, because a button is the device speaking.
    const int16_t footY = static_cast<int16_t>(bottom - toybox::kPillHeight);
    fui::ButtonProps pill;
    pill.label = model.action;
    pill.action = ActionConfirm;
    pill.text = toybox::buttonText(screen.theme());
    screen.button(pill, fui::makeRect(x, footY, width, toybox::kPillHeight));
    bottom = static_cast<int16_t>(footY - toybox::kGutter * 2);
  }
  const fui::TextStyle prose = proseStyle(screen.theme());
  fui::TextAreaProps area;
  area.text = model.text;
  area.style = prose;
  area.showCaret = false;
  fui::textArea(screen.frame(),
                fui::makeRect(x, toybox::kBodyTop, width, static_cast<int16_t>(bottom - toybox::kBodyTop)), area);
}

}  // namespace libraryui
