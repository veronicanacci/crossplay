#pragma once

// MY LIBRARY's three screen shapes: a menu of rows, a list of books, one book.
//
// Free functions over plain models, FreeInkUI and toybox tokens only. No
// renderer, no activity, no storage, so a host test can build the real screens
// and route real taps at them. See ToyboxScreen.h for why screens are written
// this way.

#include <FreeInkUI.h>

#include <vector>

#include "../ui/ToyboxScreen.h"

namespace libraryui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRow = 1,       // a menu row; value is the row's index in the WHOLE menu
  ActionOpenBook = 2,  // a book row; value is the book's index in the library
  ActionMore = 3,      // the "..." chip in a book page's band
  ActionHeart = 4,     // the favourite toggle on a book page
  // The arrows at the foot of a book page; value is -1 or +1. With two
  // sections both land on the other one.
  ActionSwitchSection = 5,
};

// The words on the screens, named so tests can ask for them by name.
extern const char* const kTitle;       // "MY LIBRARY"
extern const char* const kNoBooks;     // the empty book list
extern const char* const kNoGroups;    // an empty authors/genres/tags/series list
extern const char* const kSearchSoon;  // the search placeholder
extern const char* const kInfo;        // the foot label of a book page's first section
extern const char* const kSummary;     // and of its second

// A menu: the band, and rows of the theme's list. `items` is the current page's
// slice, so items[0] is the top row; the caller pages with menuRowsPerPage().
struct MenuModel {
  const char* title = "";
  const fui::ListItem* items = nullptr;
  int count = 0;
  int page = 0;
  int pageCount = 1;
  // Rows that are somebody's words (an author, a tag) rather than the device's
  // labels: set in the reading cut, which carries the accents Jersey does not.
  bool reading = false;
  // Drawn instead of rows when there are none.
  const char* empty = nullptr;
};

// The band the rows are stacked in, and how many the theme's rows fit in it.
fui::Rect menuBand(const fui::DeviceContext& device);
int menuRowsPerPage(const fui::DeviceContext& device, const fui::ThemeTokens& tokens);

void buildMenu(toybox::Screen& screen, const MenuModel& model);

// One book row: the title over the author, and nothing else.
struct BookRow {
  const char* title = "";
  const char* author = "";
  int16_t value = 0;
};

struct BookListModel {
  const char* title = "";    // the band: the collection's name
  const char* caption = "";  // the line under it: which books these are
  const BookRow* rows = nullptr;
  int count = 0;
  bool empty = false;  // the whole list, not just this page
  int page = 0;
  int pageCount = 1;
};

// Where the rows go: under the band and the caption line, down to the margin.
fui::Rect bookRows(const fui::DeviceContext& device);
// How many lines a title may take before it is cut with an ellipsis.
constexpr int kTitleLines = 2;
constexpr int16_t kRowGap = 4;
constexpr int kMaxRowsOnPage = 12;
// The width a row's text may run.
int16_t bookTextWidth(const fui::Rect& rows);
// How tall a row is: its title wrapped to at most kTitleLines, the author line
// under it, and padding; never shorter than the theme's row. The activity asks
// this to page and the builder asks it to draw, so the two cannot disagree.
int16_t bookRowHeight(const fui::DrawTarget& target, const fui::ThemeTokens& tokens, const char* title, int16_t width);
// The first row of every page, filling each greedily. Always at least one page.
std::vector<int> pageStarts(const fui::Rect& rows, const int16_t* heights, int count);
int pageOf(const std::vector<int>& starts, int index);

void buildBookList(toybox::Screen& screen, const BookListModel& model);

// One book. Every string may be empty and is then left out, except the title.
struct DetailModel {
  const char* band = "";  // the collection the book is in
  const char* title = "";
  const char* author = "";
  const char* isbn = "";
  const char* publisher = "";
  const char* year = "";
  const char* genre = "";
  const char* series = "";
  const char* pages = "";
  const char* language = "";
  const char* plot = "";  // empty draws the shrug
  // Personal state. `showRead` is false in the wishlist, where a book cannot
  // have been read yet and the line is not drawn.
  bool showRead = false;
  const char* readState = "";  // as library::readStateText gives it
  const char* rating = "";     // as library::ratingText gives it
  bool favourite = false;
  const char* tags = "";
  const char* notes = "";
  // Which of the two sections under the title block is showing: the fields
  // (INFO) or the plot (SUMMARY). The arrows at the foot switch them.
  bool showPlot = false;
  // Which screenful of the summary to show, for a plot longer than the room
  // left. Pinned to the last page when it asks for one past the end.
  int plotPage = 0;
};

// Where the heart sits: at the right of the title block.
fui::Rect heartRect(const fui::DeviceContext& device);
// How the summary paged: how many lines fit, how many there are, how many
// pages that makes and which one was drawn. Filled in by buildDetail, read by
// the activity to step with the side keys.
struct DetailPaging {
  int visibleLines = 0;
  int totalLines = 0;
  int pageCount = 1;
  int page = 0;
};
DetailPaging buildDetail(toybox::Screen& screen, const DetailModel& model);

// A band and one centred sentence: the search placeholder.
void buildNotice(toybox::Screen& screen, const char* title, const char* words);

}  // namespace libraryui
