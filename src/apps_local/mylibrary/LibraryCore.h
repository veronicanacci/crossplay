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
  bool read = false;
  int rating = 0;  // 0 = not rated, 1..5
  bool favourite = false;
  std::string tags;  // as typed: "gothic; horror"
  std::string notes;
  std::string location;
  // A tombstone rather than an erasure, so a later import cannot resurrect it.
  bool deleted = false;
};

// The ways into a book list from a collection screen.
enum class Browse : uint8_t { All, Authors, Genres, Tags, Series, Read, Unread, Favourites };

// Authors, Genres, Tags and Series are grouped: they show a list of values
// first and the books for one value second. The rest go straight to books.
bool isGrouped(Browse browse);

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

// 0 to 5, anything else pinned to the nearer end.
int clampRating(int rating);
// "Not rated" for 0, otherwise one asterisk per point.
const char* ratingText(int rating);

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

}  // namespace library
