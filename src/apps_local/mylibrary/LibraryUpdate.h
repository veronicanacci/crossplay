#pragma once

// Bringing BookBuddy's books into the library, and the file the library's
// bibliographic layer lives in.
//
// Two layers, one rule. BookBuddy owns the bibliographic fields and may refresh
// them; the device owns the personal fields (collection, read state, rating,
// favourite, tags, notes, location, the tombstone) and an import never touches
// them on a book the library already has. Matching is by library::stableId()
// and nothing looser: the same ISBN is the same book, and two books with
// similar titles are two books.
//
// Freestanding, like the rest of the model: the activity reads and writes the
// files, this decides what goes in them.

#include <string>
#include <vector>

#include "BookBuddyImport.h"
#include "LibraryCore.h"

namespace library {

// ---- where things are ---------------------------------------------------------
//
//     /library/imports/full/mylibrary.tsv     BookBuddy's full library export
//     /library/imports/full/wishlist.tsv      BookBuddy's full wishlist export
//     /library/imports/quick/*.tsv            a few books scanned in a shop
//     /library/imports/quick/processed/       where those go once imported
//     /library/database/library.tsv           the bibliographic layer
//     /library/state/personal.tsv             the personal layer
constexpr char kLibraryDir[] = "/library";
constexpr char kImportsDir[] = "/library/imports";
constexpr char kFullDir[] = "/library/imports/full";
constexpr char kFullLibraryFile[] = "mylibrary.tsv";
constexpr char kFullWishlistFile[] = "wishlist.tsv";
constexpr char kQuickDir[] = "/library/imports/quick";
constexpr char kProcessedDir[] = "/library/imports/quick/processed";
constexpr char kDatabaseDir[] = "/library/database";
constexpr char kDatabasePath[] = "/library/database/library.tsv";
constexpr char kStateDir[] = "/library/state";  // kPersonalPath, in LibraryCore.h

// ---- what an update did -------------------------------------------------------

struct UpdateReport {
  int found = 0;           // usable books in the import
  int added = 0;           // new to the library
  int refreshed = 0;       // already here; bibliographic fields refreshed
  int suppressed = 0;      // already here and deleted; stayed that way
  int movesPreserved = 0;  // here, and in a different collection from the export's
  int conflicts = 0;       // in both full exports; the library export won
  int skipped = 0;         // lines the parser could not read
  int myBooks = 0;         // the library afterwards
  int wishlist = 0;
};

// What a quick update would do, before it does it.
UpdateReport previewQuick(const std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& imported);

// A quick update: a few books into `destination`. A book the library has keeps
// its collection and every other personal field, and only its bibliographic
// fields are refreshed; a deleted one stays deleted; a new one is added to
// `destination` with the personal values BookBuddy carried for it.
UpdateReport applyQuick(std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& imported,
                        Collection destination);

// A complete update: both full exports at once. New books join the collection
// their export names, My Books winning a book that is in both; books the
// library has are refreshed and keep their personal state, collection included;
// books the library has that the exports no longer mention are KEPT, with the
// last bibliographic fields they had, because this library is the record and
// an export is a snapshot of another app's.
UpdateReport applyComplete(std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& fromLibrary,
                           const std::vector<bookbuddy::ImportedBook>& fromWishlist);

// Copies the bibliographic fields of `from` onto `onto`, a non-empty value at a
// time: BookBuddy losing a field is not the device forgetting it.
void refreshMetadata(const bookbuddy::ImportedBook& from, Book& onto);

// ---- the database file --------------------------------------------------------
//
// The bibliographic layer, one book a line: a version line, a header naming the
// columns, then id, isbn, title, author, publisher, year, genre, series, pages,
// language, collection, summary. The collection here is the one the import
// gave the book; once the personal file has a record for it, that wins.
// Escaping as the personal file: tabs, newlines and backslashes in a field are
// written as \t, \n and \\.
std::string encodeDatabase(const std::vector<Book>& books);
// Reads what it can. `out` is replaced; a damaged line is skipped. False when
// the text is not a database at all (no version line).
bool decodeDatabase(const std::string& text, std::vector<Book>& out);

}  // namespace library
