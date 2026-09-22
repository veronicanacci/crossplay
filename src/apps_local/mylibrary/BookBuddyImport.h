#pragma once

// BookBuddy's TSV export, read into books.
//
// The one place that knows BookBuddy's column names. Everything else in the
// app sees an ImportedBook: the bibliographic fields BookBuddy owns, and the
// handful of personal values it happens to carry, kept apart so the merge can
// treat them differently.
//
// The format, as observed in a real export of 329 books (2026-09-22):
//   * one header line naming 79 columns, tab-separated, then one line per book
//   * LF line endings, no byte-order mark, UTF-8 throughout
//   * NO quoting: a double quote is a literal character (summaries start with
//     one), a field never contains a tab or a newline
//   * every book had a 13-digit ISBN but one, which had 10 digits
//   * Wish List is 1 in the wishlist export and 0 in the library export
// Reading is by column NAME, not position, so a column BookBuddy adds or moves
// costs nothing here. Freestanding: <string> and <vector> only.

#include <string>
#include <vector>

#include "LibraryCore.h"

namespace bookbuddy {

// The columns read, spelt exactly as BookBuddy writes them in the header line.
// Named here so a test and the parser cannot disagree about them.
namespace column {
constexpr const char* kTitle = "Title";
constexpr const char* kSeries = "Series";
constexpr const char* kAuthor = "Author";
constexpr const char* kPublisher = "Publisher";
constexpr const char* kDatePublished = "Date Published";  // "2012/07/09"
constexpr const char* kYearPublished = "Year Published";  // "2012"
constexpr const char* kGenre = "Genre";                   // "Fiction, fantasy, epic"
constexpr const char* kSummary = "Summary";
constexpr const char* kPages = "Number of Pages";
constexpr const char* kLanguage = "Language";
constexpr const char* kIsbn = "ISBN";
// Personal values BookBuddy carries. Read for a book the library has never
// seen; never applied to one it has.
constexpr const char* kFavorites = "Favorites";  // "0" / "1"
constexpr const char* kRating = "Rating";        // "0.000000" .. "5.000000"
constexpr const char* kLocation = "Physical Location";
constexpr const char* kStatus = "Status";  // "Unread", "Reading", "Read"
constexpr const char* kTags = "Tags";
constexpr const char* kNotes = "Notes";
constexpr const char* kWishList = "Wish List";  // "0" / "1"
}  // namespace column

struct ImportedBook {
  // The bibliographic fields, on a Book with default personal state.
  library::Book meta;
  // What BookBuddy said about the reader's relationship to the book. Only ever
  // used to seed a NEW record; a book the library already knows keeps its own.
  bool favourite = false;
  int rating = 0;  // BookBuddy's float, rounded
  library::ReadState state = library::ReadState::Unread;
  std::string location;
  std::string tags;
  std::string notes;
  // BookBuddy's own Wish List flag, for the record.
  bool wishlist = false;
};

struct ParseResult {
  std::vector<ImportedBook> books;
  // Lines that were not a book: the wrong number of fields, or no title and no
  // ISBN. Reported, never fatal.
  int skipped = 0;
  // Set when the text is not a BookBuddy export at all: no header line, or a
  // header without a Title column. `books` is then empty.
  bool invalid = false;
  std::string error;
};

// Reads a whole export. Never throws, never crashes on junk: an unreadable file
// comes back `invalid` with a reason, a damaged line is skipped and counted.
ParseResult parse(const std::string& text);

// Whether a file in the quick folder is one to offer: ".tsv" or ".txt", any case.
bool isTsvName(const std::string& name);

}  // namespace bookbuddy
