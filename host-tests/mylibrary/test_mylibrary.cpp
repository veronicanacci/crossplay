// MY LIBRARY's model, driven without a device.
//
// What is worth testing here is what a screenshot cannot show: that a search
// for "suskind" finds Süskind and "978 014" finds 978-0-14, that an author
// spelt two ways is one author, that a deleted book is in no list at all, and
// that what is written to the card comes back as what was written.

#include <cstdio>
#include <string>
#include <vector>

#include "../../src/apps_local/mylibrary/LibraryCore.h"

namespace {

int checks = 0;
int failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++checks;                                                     \
    if (!(cond)) {                                                \
      ++failures;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

using library::Book;
using library::Browse;
using library::Collection;
using library::Filter;
using library::ReadState;

Book book(const char* title, const char* author, const char* isbn = "", const char* genre = "",
          const char* series = "") {
  Book b;
  b.title = title;
  b.author = author;
  b.isbn = isbn;
  b.genre = genre;
  b.series = series;
  return b;
}

// A small library with the cases the tests need: an accented author, a
// duplicated author in different capitals, a series, a wishlist book, a
// deleted book.
std::vector<Book> fixture() {
  std::vector<Book> books;
  books.push_back(book("If on a winter's night a traveler", "Italo Calvino", "978-0-15-643961-9", "Fiction"));
  books.push_back(book("Invisible Cities", "italo calvino", "9780156453806", "Fiction"));
  books.push_back(book("Perfume", "Patrick Süskind", "9780140120837", "Horror"));
  books.push_back(book("Dune", "Frank Herbert", "9780441172719", "Science Fiction", "Dune"));
  books.push_back(book("Dune Messiah", "Frank Herbert", "9780441172696", "Science Fiction", "Dune"));
  books.push_back(book("Dracula", "Bram Stoker", "9780141439846", "Horror"));
  books.push_back(book("Piranesi", "Susanna Clarke", "9781635575637", "Fantasy"));
  books.push_back(book("Zafón's Shadow", "Carlos Ruiz Zafón", "9780143034902", "Mystery", "Forgotten Books"));

  books[0].state = ReadState::Read;
  books[0].favourite = true;
  books[0].tags = "Italy; metafiction";
  books[2].state = ReadState::Read;
  books[2].tags = "gothic; weird little man";
  books[5].tags = "gothic; italy";
  books[5].favourite = true;
  books[6].collection = Collection::Wishlist;
  books[6].favourite = true;
  books[6].tags = "labyrinth";
  books[7].collection = Collection::Wishlist;
  books[7].deleted = true;
  // A deleted owned book too, tagged and favourited, which must show nowhere.
  books.push_back(book("Ghost", "Bram Stoker", "9780000000001", "Horror", "Dune"));
  books.back().deleted = true;
  books.back().favourite = true;
  books.back().tags = "gothic";
  books.back().state = ReadState::Read;
  return books;
}

Filter filter(const Browse browse, const char* value = "", const Collection collection = Collection::MyBooks) {
  Filter f;
  f.collection = collection;
  f.browse = browse;
  f.value = value;
  return f;
}

std::vector<std::string> titles(const std::vector<Book>& books, const Filter& f) {
  std::vector<std::string> out;
  for (const int i : library::select(books, f)) out.push_back(books[i].title);
  return out;
}

bool same(const std::vector<std::string>& got, const std::vector<std::string>& want) {
  if (got.size() != want.size()) return false;
  for (size_t i = 0; i < got.size(); ++i) {
    if (got[i] != want[i]) return false;
  }
  return true;
}

// ---- search -------------------------------------------------------------------

void testTitleSearchIsCaseInsensitiveAndPartial() {
  const std::vector<Book> books = fixture();
  CHECK(same(titles(books, filter(Browse::SearchTitle, "winter")), {"If on a winter's night a traveler"}));
  CHECK(same(titles(books, filter(Browse::SearchTitle, "WINTER")), {"If on a winter's night a traveler"}));
  CHECK(same(titles(books, filter(Browse::SearchTitle, "dune")), {"Dune", "Dune Messiah"}));
  CHECK(titles(books, filter(Browse::SearchTitle, "nothing like this")).empty());
  // Nothing typed finds nothing, not everything.
  CHECK(titles(books, filter(Browse::SearchTitle, "")).empty());
}

void testTitleSearchFoldsAccents() {
  std::vector<Book> books = fixture();
  books[7].deleted = false;
  CHECK(same(titles(books, filter(Browse::SearchTitle, "zafon", Collection::Wishlist)), {"Zafón's Shadow"}));
  CHECK(same(titles(books, filter(Browse::SearchTitle, "ZAFÓN", Collection::Wishlist)), {"Zafón's Shadow"}));
}

void testAuthorSearchIsCaseInsensitiveAndPartial() {
  const std::vector<Book> books = fixture();
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "calv")),
             {"If on a winter's night a traveler", "Invisible Cities"}));
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "CALVINO")),
             {"If on a winter's night a traveler", "Invisible Cities"}));
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "herb")), {"Dune", "Dune Messiah"}));
}

void testAuthorSearchFoldsAccents() {
  const std::vector<Book> books = fixture();
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "suskind")), {"Perfume"}));
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "Süskind")), {"Perfume"}));
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "SÜSKIND")), {"Perfume"}));
}

void testIsbnSearchIgnoresSpacesAndHyphens() {
  const std::vector<Book> books = fixture();
  // The book's own ISBN carries hyphens; the query may carry either or neither.
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "9780156439619")), {"If on a winter's night a traveler"}));
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "978-0-15-643961-9")), {"If on a winter's night a traveler"}));
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "978 0 15 643961 9")), {"If on a winter's night a traveler"}));
  // A partial query, spelt three ways, is the same prefix.
  const std::vector<std::string> penguin = {"Dracula", "Perfume"};
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "978014")), penguin));
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "978-014")), penguin));
  CHECK(same(titles(books, filter(Browse::SearchIsbn, "978 014")), penguin));
  CHECK(titles(books, filter(Browse::SearchIsbn, "")).empty());
  CHECK(titles(books, filter(Browse::SearchIsbn, " - ")).empty());
}

void testSearchRespectsTheCollection() {
  const std::vector<Book> books = fixture();
  // Piranesi is on the wishlist: found there, not among the owned books.
  CHECK(titles(books, filter(Browse::SearchTitle, "piranesi")).empty());
  CHECK(same(titles(books, filter(Browse::SearchTitle, "piranesi", Collection::Wishlist)), {"Piranesi"}));
  CHECK(titles(books, filter(Browse::SearchAuthor, "clarke")).empty());
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "clarke", Collection::Wishlist)), {"Piranesi"}));
  // And the owned books are not on the wishlist.
  CHECK(titles(books, filter(Browse::SearchTitle, "dune", Collection::Wishlist)).empty());
}

void testDeletedBooksAreNotFoundBySearch() {
  const std::vector<Book> books = fixture();
  CHECK(titles(books, filter(Browse::SearchTitle, "ghost")).empty());
  CHECK(titles(books, filter(Browse::SearchIsbn, "9780000000001")).empty());
  CHECK(titles(books, filter(Browse::SearchTitle, "zafon", Collection::Wishlist)).empty());
  // The author search still finds Stoker's other book and only that.
  CHECK(same(titles(books, filter(Browse::SearchAuthor, "stoker")), {"Dracula"}));
}

// ---- browse -------------------------------------------------------------------

void testAuthorsAreUniqueWhateverTheirCapitals() {
  const std::vector<Book> books = fixture();
  const std::vector<std::string> authors = library::groups(books, Collection::MyBooks, Browse::Authors);
  // Calvino once, spelt the way he was first seen; Stoker once, the deleted
  // Ghost notwithstanding; alphabetical.
  CHECK(same(authors, {"Bram Stoker", "Frank Herbert", "Italo Calvino", "Patrick Süskind"}));
  CHECK(same(library::groups(books, Collection::MyBooks, Browse::Genres), {"Fiction", "Horror", "Science Fiction"}));
  CHECK(same(library::groups(books, Collection::MyBooks, Browse::Series), {"Dune"}));
  CHECK(same(library::groups(books, Collection::Wishlist, Browse::Authors), {"Susanna Clarke"}));
  CHECK(same(library::groups(books, Collection::Wishlist, Browse::Series), {}));
}

void testTagsGroupCaseInsensitively() {
  const std::vector<Book> books = fixture();
  // "Italy" and "italy" are one tag; the deleted Ghost adds nothing.
  CHECK(same(library::groups(books, Collection::MyBooks, Browse::Tags),
             {"gothic", "Italy", "metafiction", "weird little man"}));
  CHECK(same(library::groups(books, Collection::Wishlist, Browse::Tags), {"labyrinth"}));
}

void testFilteringByGroupValue() {
  const std::vector<Book> books = fixture();
  CHECK(same(titles(books, filter(Browse::Authors, "Italo Calvino")),
             {"If on a winter's night a traveler", "Invisible Cities"}));
  // The group's spelling is one of the two; either finds both.
  CHECK(same(titles(books, filter(Browse::Authors, "italo calvino")),
             {"If on a winter's night a traveler", "Invisible Cities"}));
  CHECK(same(titles(books, filter(Browse::Genres, "Horror")), {"Dracula", "Perfume"}));
  CHECK(same(titles(books, filter(Browse::Series, "Dune")), {"Dune", "Dune Messiah"}));
  CHECK(same(titles(books, filter(Browse::Tags, "gothic")), {"Dracula", "Perfume"}));
  CHECK(same(titles(books, filter(Browse::Tags, "ITALY")), {"Dracula", "If on a winter's night a traveler"}));
}

void testReadUnreadAndFavourites() {
  const std::vector<Book> books = fixture();
  CHECK(same(titles(books, filter(Browse::Read)), {"If on a winter's night a traveler", "Perfume"}));
  CHECK(same(titles(books, filter(Browse::Unread)), {"Dracula", "Dune", "Dune Messiah", "Invisible Cities"}));
  CHECK(same(titles(books, filter(Browse::Favourites)), {"Dracula", "If on a winter's night a traveler"}));
  CHECK(same(titles(books, filter(Browse::Favourites, "", Collection::Wishlist)), {"Piranesi"}));
}

void testDeletedBooksAreInNoBrowse() {
  const std::vector<Book> books = fixture();
  // Ghost is read, favourited, tagged gothic, in the Dune series and Horror,
  // by Stoker. Nothing lists it.
  const Browse browses[] = {Browse::All, Browse::Read, Browse::Favourites};
  for (const Browse browse : browses) {
    for (const std::string& title : titles(books, filter(browse))) CHECK(title != "Ghost");
  }
  for (const std::string& title : titles(books, filter(Browse::Authors, "Bram Stoker"))) CHECK(title != "Ghost");
  for (const std::string& title : titles(books, filter(Browse::Series, "Dune"))) CHECK(title != "Ghost");
  for (const std::string& title : titles(books, filter(Browse::Tags, "gothic"))) CHECK(title != "Ghost");
  for (const std::string& title : titles(books, filter(Browse::Genres, "Horror"))) CHECK(title != "Ghost");
  CHECK(titles(books, filter(Browse::All, "", Collection::Wishlist)).size() == 1);
  CHECK(same(titles(books, filter(Browse::All)),
             {"Dracula", "Dune", "Dune Messiah", "If on a winter's night a traveler", "Invisible Cities", "Perfume"}));
}

// ---- tags ---------------------------------------------------------------------

void testTagParsing() {
  const std::vector<std::string> tags = library::splitTags("gothic; horror ;  ; italy;weird little man; ");
  CHECK(same(tags, {"gothic", "horror", "italy", "weird little man"}));
  CHECK(library::splitTags("").empty());
  CHECK(library::splitTags(" ; ; ").empty());
  CHECK(same(library::splitTags("one"), {"one"}));
}

// ---- normalisation ------------------------------------------------------------

void testFolding() {
  CHECK(library::fold("Süskind") == "suskind");
  CHECK(library::fold("Zafón") == "zafon");
  CHECK(library::fold("ÀÉÎÕÜ Çñ ß Æ Œ") == "aeiou cn ss ae oe");
  CHECK(library::fold("Łódź") == "lodz");
  CHECK(library::fold("Plain ASCII 123") == "plain ascii 123");
  // Letters the tables do not cover pass through rather than vanish.
  CHECK(library::fold("Ωmega") == "Ωmega");
  CHECK(library::foldIsbn("978-0-14-118776-1") == "9780141187761");
  CHECK(library::foldIsbn("978 0 14 118776 1") == "9780141187761");
  CHECK(library::foldIsbn("9780141187761") == "9780141187761");
}

// ---- identity -----------------------------------------------------------------

void testStableIds() {
  Book a = book("Dune", "Frank Herbert", "978-0-441-17271-9");
  Book b = book("Dune", "Frank Herbert", "9780441172719");
  CHECK(library::stableId(a) == "isbn:9780441172719");
  CHECK(library::stableId(a) == library::stableId(b));
  // No ISBN: the folded metadata, so capitals and accents do not make two ids.
  Book c = book("Perfume", "Patrick Süskind");
  Book d = book("PERFUME", "patrick suskind");
  CHECK(library::stableId(c).rfind("meta:", 0) == 0);
  CHECK(library::stableId(c) == library::stableId(d));
  Book e = book("Perfume", "Somebody Else");
  CHECK(library::stableId(c) != library::stableId(e));
}

// ---- persistence --------------------------------------------------------------

void testPersonalStateRoundTrip() {
  std::vector<Book> before = fixture();
  before[1].notes = "A note with\ta tab\nand a line break \\ and a backslash";
  before[1].location = "Bedroom shelf 2";
  before[1].rating = 4;
  before[3].collection = Collection::Wishlist;  // moved, with its read state kept
  before[3].state = ReadState::Dnf;
  before[3].location = "Storage box";

  const std::string text = library::encodePersonal(before, {});
  CHECK(text.rfind("# CrossPlay MY LIBRARY personal v1\n", 0) == 0);

  std::vector<library::PersonalRecord> records;
  library::decodePersonal(text, records);
  CHECK(records.size() == before.size());

  std::vector<Book> after = fixture();
  // A fresh fixture has none of the edits; the file puts them back.
  after[1].tags.clear();
  after[3].collection = Collection::MyBooks;
  std::vector<library::PersonalRecord> others;
  library::loadPersonal(records, after, others);
  CHECK(others.empty());
  for (size_t i = 0; i < before.size(); ++i) {
    CHECK(after[i].collection == before[i].collection);
    CHECK(after[i].state == before[i].state);
    CHECK(after[i].rating == before[i].rating);
    CHECK(after[i].favourite == before[i].favourite);
    CHECK(after[i].deleted == before[i].deleted);
    CHECK(after[i].tags == before[i].tags);
    CHECK(after[i].notes == before[i].notes);
    CHECK(after[i].location == before[i].location);
    // And the bibliographic fields were never in the file.
    CHECK(after[i].title == before[i].title);
  }
  // The specific things V2 promises survive a reload.
  CHECK(after[0].tags == "Italy; metafiction");
  CHECK(after[1].notes == before[1].notes);
  CHECK(after[1].location == "Bedroom shelf 2");
  CHECK(after[3].collection == Collection::Wishlist);
  CHECK(after[3].state == ReadState::Dnf);
  CHECK(after[7].deleted);
  CHECK(after[8].deleted);
  CHECK(titles(after, filter(Browse::All)).size() == 5);
  CHECK(same(titles(after, filter(Browse::All, "", Collection::Wishlist)), {"Dune", "Piranesi"}));
}

void testRecordsForOtherBooksAreKept() {
  std::vector<Book> books = fixture();
  library::PersonalRecord stray;
  stray.id = "isbn:9999999999999";
  stray.tags = "kept";
  stray.deleted = true;
  const std::string text = library::encodePersonal(books, {stray});
  std::vector<library::PersonalRecord> records;
  library::decodePersonal(text, records);
  std::vector<library::PersonalRecord> others;
  library::loadPersonal(records, books, others);
  CHECK(others.size() == 1);
  CHECK(others.size() == 1 && others[0].id == "isbn:9999999999999" && others[0].tags == "kept" && others[0].deleted);
}

void testDamagedFileCostsOnlyItsDamagedLines() {
  std::vector<Book> books = fixture();
  std::string text = "# CrossPlay MY LIBRARY personal v1\n";
  text += "not a record at all\n";
  text += "\tmybooks\tread\t5\t1\t0\n";                                                   // no id
  text += library::stableId(books[5]) + "\twishlist\tdnf\t9\t1\t1\tx; y\tShelf\tNote\n";  // rating out of range
  text += library::stableId(books[3]) + "\tmybooks\tread\r\n";                            // short, CRLF, no rating
  text += "\n";
  std::vector<library::PersonalRecord> records;
  library::decodePersonal(text, records);
  CHECK(records.size() == 1);
  std::vector<library::PersonalRecord> others;
  library::loadPersonal(records, books, others);
  CHECK(books[5].collection == Collection::Wishlist);
  CHECK(books[5].state == ReadState::Dnf);
  CHECK(books[5].rating == 5);
  CHECK(books[5].deleted);
  CHECK(books[5].tags == "x; y");
  CHECK(books[5].notes == "Note");
  // The short line was skipped and Dune is as the fixture made it.
  CHECK(books[3].state == ReadState::Unread);
  // An empty file, or none, is a library as the fixture has it.
  library::decodePersonal("", records);
  CHECK(records.empty());
}

}  // namespace

int main() {
  testTitleSearchIsCaseInsensitiveAndPartial();
  testTitleSearchFoldsAccents();
  testAuthorSearchIsCaseInsensitiveAndPartial();
  testAuthorSearchFoldsAccents();
  testIsbnSearchIgnoresSpacesAndHyphens();
  testSearchRespectsTheCollection();
  testDeletedBooksAreNotFoundBySearch();
  testAuthorsAreUniqueWhateverTheirCapitals();
  testTagsGroupCaseInsensitively();
  testFilteringByGroupValue();
  testReadUnreadAndFavourites();
  testDeletedBooksAreInNoBrowse();
  testTagParsing();
  testFolding();
  testStableIds();
  testPersonalStateRoundTrip();
  testRecordsForOtherBooksAreKept();
  testDamagedFileCostsOnlyItsDamagedLines();
  std::printf("mylibrary: %d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
