#pragma once

// MY LIBRARY: the books, what the device knows about each one that BookBuddy
// does not, and the ways of picking a subset to show.
//
// Freestanding: <string> and <vector> and nothing else, so host-tests can build
// the whole model without a device. Milestone 1 holds the library in memory
// and seeds it from sampleBooks(); the card comes later.

#include <cstdint>
#include <string>
#include <vector>

namespace library {

enum class Collection : uint8_t { MyBooks, Wishlist };

// Whether a book has been read. Three states rather than a flag: a book put
// down for good is neither read nor waiting to be.
enum class ReadState : uint8_t { Unread, Read, Dnf };

struct Book {
  // Imported (bibliographic) metadata. BookBuddy's to fill; any field may be
  // empty and every screen has to cope with that.
  std::string title;
  std::string author;
  std::string isbn;
  std::string publisher;
  std::string year;
  std::string genre;
  std::string series;
  std::string pages;
  std::string language;
  std::string plot;

  // Personal state, owned by the device. Every field independent of the others:
  // reading a book does not rate it, favouriting does not read it, moving it
  // between collections changes nothing but the collection.
  Collection collection = Collection::MyBooks;
  ReadState state = ReadState::Unread;
  int rating = 0;  // 0 = not rated, 1..5
  bool favourite = false;
  std::string tags;  // as typed: "gothic; horror"
  std::string notes;
  std::string location;
  // A tombstone rather than an erasure, so a later import cannot resurrect it.
  bool deleted = false;
};

// The ways into a book list from a collection screen. The last three are the
// search modes: only the named field is searched, and `Filter::value` is the
// typed query.
enum class Browse : uint8_t {
  All,
  Authors,
  Genres,
  Tags,
  Series,
  Read,
  Unread,
  Favourites,
  SearchTitle,
  SearchAuthor,
  SearchIsbn,
};

// Authors, Genres, Tags and Series are grouped: they show a list of values
// first and the books for one value second. The rest go straight to books.
bool isGrouped(Browse browse);
bool isSearch(Browse browse);

// Which books a list shows. `value` is the group picked, for a grouped browse.
struct Filter {
  Collection collection = Collection::MyBooks;
  Browse browse = Browse::All;
  std::string value;
};

// A few real books, so the screens have something to show before the card does.
std::vector<Book> sampleBooks();

const char* collectionTitle(Collection collection);  // "MY BOOKS" / "WISHLIST"
const char* browseTitle(Browse browse);              // "ALL BOOKS", "AUTHORS", ...
// What an empty screen says: for the list of values a grouped browse offers,
// and for a list of books.
const char* groupsEmptyText(Browse browse);
const char* booksEmptyText(Browse browse);

// 0 to 5, anything else pinned to the nearer end.
int clampRating(int rating);
// "Not rated" for 0, otherwise one asterisk per point.
const char* ratingText(int rating);
// "Unread", "Read" or "DNF".
const char* readStateText(ReadState state);
// "Unread", "Read" or "DNF".
const char* readStateText(ReadState state);

// "gothic; horror ; ;italy" -> {"gothic", "horror", "italy"}: split on
// semicolons, whitespace trimmed, empties dropped.
std::vector<std::string> splitTags(const std::string& text);

// Whether `book` belongs in the list `filter` describes. Deleted books never do.
bool matches(const Book& book, const Filter& filter);

// Indexes into `books` of everything matching, in title order.
std::vector<int> select(const std::vector<Book>& books, const Filter& filter);

// The distinct values a grouped browse offers within a collection (every author,
// every tag...), sorted, deleted books excluded.
std::vector<std::string> groups(const std::vector<Book>& books, Collection collection, Browse browse);

// `page` moved by `delta`, stopping at both ends.
int pageStep(int page, int pageCount, int delta);

// ---- normalisation ----------------------------------------------------------

// The text as it is compared: ASCII lowercased, and the Latin letters with
// diacritics (U+00C0 to U+017F) folded to their base letters, so "suskind"
// finds Süskind and "zafon" finds Zafón. Everything else passes through
// unchanged. A table of two hundred code points rather than a Unicode
// library, because that is every accent a library of Latin-script books has.
std::string fold(const std::string& text);

// An ISBN as it is compared: spaces and hyphens removed, so 978-0-14 and
// 978 0 14 and 9780 14 are the same prefix. Nothing else is touched; a partial
// query is a substring of this.
std::string foldIsbn(const std::string& text);

// ---- identity -----------------------------------------------------------------

// The key personal state is filed under. "isbn:" and the folded ISBN when the
// book has one of the right length; otherwise "meta:" and a hash of the folded
// author, title, publisher and year, which is the same for the same book
// exported twice and is what lets a later import find its personal state.
std::string stableId(const Book& book);

// ---- the personal state file ------------------------------------------------

// Beside the reader's own state and every game's save, so clearing
// `.crosspoint/` clears this too. Personal state only: the bibliographic
// fields come from elsewhere (sampleBooks() today, an import later) and a
// refresh of them must not touch this file.
constexpr char kPersonalPath[] = "/.crosspoint/mylibrary-personal.tsv";

// One book's personal state, keyed by stableId(). What the file holds, one
// record a line, and what a record for a book the library no longer has is
// kept as, so a book that comes back finds its state waiting.
struct PersonalRecord {
  std::string id;
  Collection collection = Collection::MyBooks;
  ReadState state = ReadState::Unread;
  int rating = 0;
  bool favourite = false;
  bool deleted = false;
  std::string tags;
  std::string location;
  std::string notes;
};

PersonalRecord personalOf(const Book& book);
// Copies a record's fields onto a book. The id is not checked here.
void applyPersonal(const PersonalRecord& record, Book& book);

// The file: a version line, then one tab-separated record a line:
//
//     # CrossPlay MY LIBRARY personal v1
//     id  collection  state  rating  favourite  deleted  tags  location  notes
//
// Tabs, newlines and backslashes inside a text field are written as \\t, \\n
// and \\\\, so a record is always one line. `others` are records for books
// not in `books`, written back unchanged.
std::string encodePersonal(const std::vector<Book>& books, const std::vector<PersonalRecord>& others);

// Reads what it can and skips what it cannot: a line that is not a record,
// a record with no id, a field it does not understand. Never fails; a
// damaged file costs its damaged lines. `out` is replaced.
void decodePersonal(const std::string& text, std::vector<PersonalRecord>& out);

// Puts every record onto the book it is for; the rest come back in `others`.
void loadPersonal(const std::vector<PersonalRecord>& records, std::vector<Book>& books,
                  std::vector<PersonalRecord>& others);

}  // namespace library
