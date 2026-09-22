// BookBuddy's export read, and the library updated from it, without a device.
//
// The fixture is the shape of a real export seen on 2026-09-22: its exact
// 79-column header, tab-separated, LF-terminated, unquoted, with rows built by
// column NAME so the tests cannot silently depend on a column's position. The
// books in it are a few public titles, not anybody's library.
//
// What is worth testing: that a real row comes back as the book it describes;
// that a damaged file skips its damaged lines and a non-export is refused; that
// identity is exact ISBN and nothing looser; and that an update refreshes what
// BookBuddy owns and leaves what the device owns alone: a rating, a tag, a
// favourite, a read state, a location, a move between collections, a deletion,
// and a book the export no longer mentions.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "../../src/apps_local/mylibrary/BookBuddyImport.h"
#include "../../src/apps_local/mylibrary/LibraryCore.h"
#include "../../src/apps_local/mylibrary/LibraryUpdate.h"

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
using library::Collection;
using library::ReadState;

// BookBuddy's header line, verbatim.
const char* const kHeader =
    "Title\tOriginal Title\tSubtitle\tSeries\tVolume\tAuthor\tAuthor (Last, First)\tIllustrator\tNarrator\tTranslator\t"
    "Photographer\tEditor\tPublisher\tPlace of Publication\tDate Published\tYear Published\tOriginal Date Published\t"
    "Original Year Published\tEdition\tGenre\tSummary\tGuided Reading Level\tLexile Measure\tLexile Code\t"
    "Grade Level Equivalent\tDevelopmental Reading Assessment\tInterest Level\tAR Level\tAR Points\tAR Quiz Number\t"
    "Word Count\tNumber of Pages\tFormat\tAudio Runtime\tDimensions\tWeight\tList Price\tLanguage\tOriginal Language\t"
    "DDC\tLCC\tLCCN\tOCLC\tISBN\tISSN\tFavorites\tRating\tPhysical Location\tStatus\tStatus Incompleted Reason\t"
    "Status Hidden\tDate Started\tDate Finished\tCurrent Page\tLoaned To\tDate Loaned\tBorrowed From\tDate Borrowed\t"
    "Returned from Borrow\tNot Owned Reason\tQuantity\tCondition\tRecommended By\tDate Added\tUser Supplied ID\t"
    "User Supplied Descriptor\tTags\tPurchase Date\tPurchase Place\tPurchase Price\tNotes\tGoogle VolumeID\tCategory\t"
    "Wish List\tPreviously Owned\tUp Next\tPosition\tUploaded Image URL\tActivities";

std::vector<std::string> split(const std::string& line) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= line.size()) {
    size_t end = line.find('\t', start);
    if (end == std::string::npos) end = line.size();
    out.push_back(line.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

// A row with the given columns filled and every other one empty, the way
// BookBuddy writes a book it knows little about.
std::string row(const std::map<std::string, std::string>& values) {
  const std::vector<std::string> header = split(kHeader);
  std::string out;
  for (size_t i = 0; i < header.size(); ++i) {
    if (i > 0) out.push_back('\t');
    const auto it = values.find(header[i]);
    if (it != values.end()) out += it->second;
  }
  return out;
}

// Three books as a real export carries them: a favourite rated five and being
// read, a plain unread one whose year is only in Date Published, and one with
// a 10-digit ISBN and an accented author.
std::map<std::string, std::string> lifeCeremony() {
  return {{"Title", "Life Ceremony"},
          {"Author", "Sayaka Murata"},
          {"Author (Last, First)", "Murata, Sayaka"},
          {"Publisher", "Granta Books"},
          {"Date Published", "2022/07/07"},
          {"Year Published", "2022"},
          {"Genre", "Fiction"},
          {"Summary",
           "\"With Life Ceremony, the incomparable Sayaka Murata returns with a brilliant and wonderfully "
           "unsettling collection\" -- it's the kind of book that ends with a \"quote\"."},
          {"Number of Pages", "266"},
          {"Language", "English"},
          {"ISBN", "9781783787371"},
          {"Favorites", "1"},
          {"Rating", "5.000000"},
          {"Status", "Reading"},
          {"Date Added", "2025/07/05 11:18:02.255349040"},
          {"Wish List", "0"},
          {"Previously Owned", "0"},
          {"Up Next", "0"}};
}

std::map<std::string, std::string> colourOfMagic() {
  return {{"Title", "The Colour of Magic"},
          {"Subtitle", "Discworld Novel 1"},
          {"Author", "Terry Pratchett"},
          {"Publisher", "Random House"},
          {"Date Published", "2012/06/14"},
          {"Genre", "Fiction, fantasy, general"},
          {"Number of Pages", "288"},
          {"Language", "English"},
          {"ISBN", "9780552166591"},
          {"Favorites", "0"},
          {"Rating", "0.000000"},
          {"Status", "Unread"},
          {"Wish List", "0"}};
}

std::map<std::string, std::string> perfume() {
  return {{"Title", "Il profumo"},    {"Author", "Patrick Süskind"},
          {"Publisher", "TEA"},       {"Year Published", "1985"},
          {"Genre", "Fiction"},       {"Summary", "Jean-Baptiste Grenouille nasce a Parigi senza odore proprio."},
          {"Number of Pages", "263"}, {"Language", "Italian"},
          {"ISBN", "8850200080"},     {"Favorites", "0"},
          {"Rating", "0.000000"},     {"Status", "Read"},
          {"Wish List", "0"}};
}

std::string exportOf(const std::vector<std::map<std::string, std::string>>& books, const char* eol = "\n") {
  std::string text = std::string(kHeader) + eol;
  for (const auto& book : books) text += row(book) + eol;
  return text;
}

int indexOf(const std::vector<Book>& books, const char* title) {
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i].title == title) return static_cast<int>(i);
  }
  return -1;
}

// ---- the parser -----------------------------------------------------------------

void testARealRowBecomesTheBookItDescribes() {
  const bookbuddy::ParseResult result = bookbuddy::parse(exportOf({lifeCeremony()}));
  CHECK(!result.invalid);
  CHECK(result.skipped == 0);
  CHECK(result.books.size() == 1);
  if (result.books.size() != 1) return;
  const bookbuddy::ImportedBook& b = result.books[0];
  CHECK(b.meta.title == "Life Ceremony");
  CHECK(b.meta.author == "Sayaka Murata");
  CHECK(b.meta.publisher == "Granta Books");
  CHECK(b.meta.year == "2022");
  CHECK(b.meta.genre == "Fiction");
  CHECK(b.meta.pages == "266");
  CHECK(b.meta.language == "English");
  CHECK(b.meta.isbn == "9781783787371");
  // The summary keeps its quotes: they are text, not quoting.
  CHECK(b.meta.plot.rfind("\"With Life Ceremony", 0) == 0);
  CHECK(b.meta.plot.find("\"quote\".") != std::string::npos);
  // The personal columns BookBuddy carries are read, for a new book to start from.
  CHECK(b.favourite);
  CHECK(b.rating == 5);
  CHECK(b.state == ReadState::Unread);  // "Reading" is not read
  CHECK(!b.wishlist);
}

void testEmptyOptionalColumnsAndTheYearFromTheDate() {
  const bookbuddy::ParseResult result = bookbuddy::parse(exportOf({colourOfMagic()}));
  CHECK(result.books.size() == 1);
  if (result.books.empty()) return;
  const bookbuddy::ImportedBook& b = result.books[0];
  CHECK(b.meta.year == "2012");  // Year Published empty; Date Published "2012/06/14"
  CHECK(b.meta.series.empty());
  CHECK(b.meta.plot.empty());
  CHECK(b.meta.genre == "Fiction, fantasy, general");
  CHECK(b.rating == 0);
  CHECK(!b.favourite);
  CHECK(b.location.empty() && b.tags.empty() && b.notes.empty());
}

void testAccentedTextSurvivesAndReadIsRead() {
  const bookbuddy::ParseResult result = bookbuddy::parse(exportOf({perfume()}));
  CHECK(result.books.size() == 1);
  if (result.books.empty()) return;
  CHECK(result.books[0].meta.author == "Patrick Süskind");
  CHECK(result.books[0].meta.language == "Italian");
  CHECK(result.books[0].state == ReadState::Read);
}

void testALongSummaryAndBothLineEndings() {
  std::map<std::string, std::string> book = lifeCeremony();
  std::string plot;
  while (plot.size() < 4000) plot += "A sentence of the summary, of which a real one has about a thousand. ";
  plot.pop_back();  // fields come back trimmed, and a real summary ends in a full stop
  book["Summary"] = plot;
  for (const char* eol : {"\n", "\r\n"}) {
    const bookbuddy::ParseResult result = bookbuddy::parse(exportOf({book, colourOfMagic()}, eol));
    CHECK(result.books.size() == 2);
    if (result.books.size() == 2) {
      CHECK(result.books[0].meta.plot == plot);
      CHECK(result.books[1].meta.title == "The Colour of Magic");
    }
  }
  // A byte-order mark on the header is stepped over.
  const bookbuddy::ParseResult bom = bookbuddy::parse("\xEF\xBB\xBF" + exportOf({perfume()}));
  CHECK(bom.books.size() == 1);
}

void testMalformedInputFailsSafely() {
  // A line with the wrong number of fields is skipped, and the rest are read.
  std::string text = exportOf({lifeCeremony()});
  text += "Only a title\tand two fields\n";
  text += row(colourOfMagic()) + "\n";
  text += "\n";
  bookbuddy::ParseResult result = bookbuddy::parse(text);
  CHECK(!result.invalid);
  CHECK(result.books.size() == 2);
  CHECK(result.skipped == 1);
  // A row with neither title nor ISBN is not a book.
  result = bookbuddy::parse(exportOf({{{"Author", "Nobody"}}}));
  CHECK(result.books.empty());
  CHECK(result.skipped == 1);
  // Not an export at all: refused, with a reason, and no books.
  result = bookbuddy::parse("");
  CHECK(result.invalid);
  result = bookbuddy::parse("this is a poem\nabout nothing\n");
  CHECK(result.invalid);
  CHECK(!result.error.empty());
  result = bookbuddy::parse("Author\tPublisher\nSomebody\tSomewhere\n");
  CHECK(result.invalid);
  // A header alone is a valid export of nothing.
  result = bookbuddy::parse(std::string(kHeader) + "\n");
  CHECK(!result.invalid);
  CHECK(result.books.empty());
  // A column that moved is still found by name.
  result = bookbuddy::parse("ISBN\tAuthor\tTitle\n9780552166591\tTerry Pratchett\tThe Colour of Magic\n");
  CHECK(result.books.size() == 1 && result.books[0].meta.title == "The Colour of Magic" &&
        result.books[0].meta.isbn == "9780552166591");
}

void testQuickFileNames() {
  CHECK(bookbuddy::isTsvName("bookshop.tsv"));
  CHECK(bookbuddy::isTsvName("Waterstones.TSV"));
  CHECK(bookbuddy::isTsvName("scan.txt"));
  CHECK(!bookbuddy::isTsvName("library.tsv.bak"));
  CHECK(!bookbuddy::isTsvName("cover.png"));
  CHECK(!bookbuddy::isTsvName("processed"));
}

// ---- identity -----------------------------------------------------------------

void testIdentityIsExactIsbnWhateverItsFormatting() {
  Book a;
  a.isbn = "978-1-78378-737-1";
  Book b;
  b.isbn = "9781783787371";
  Book c;
  c.isbn = "978 1 78378 737 1";
  CHECK(library::stableId(a) == "isbn:9781783787371");
  CHECK(library::stableId(a) == library::stableId(b));
  CHECK(library::stableId(a) == library::stableId(c));
  // A 10-digit ISBN is the same book as its 13-digit form.
  Book ten;
  ten.isbn = "8850200080";
  Book thirteen;
  thirteen.isbn = "9788850200085";
  CHECK(library::stableId(ten) == "isbn:9788850200085");
  CHECK(library::stableId(ten) == library::stableId(thirteen));
  // A different ISBN is a different book, however alike the titles.
  Book d;
  d.title = "Dune";
  d.isbn = "9780441172719";
  Book e;
  e.title = "Dune";
  e.isbn = "9780441013593";
  CHECK(library::stableId(d) != library::stableId(e));
}

void testFallbackIdentityIsDeterministicAndNotFuzzy() {
  Book a;
  a.title = "The Colour of Magic";
  a.author = "Terry Pratchett";
  a.publisher = "Random House";
  a.year = "2012";
  Book b = a;
  b.title = "THE COLOUR OF MAGIC";
  b.author = "terry pratchett";
  CHECK(library::stableId(a).rfind("meta:", 0) == 0);
  CHECK(library::stableId(a) == library::stableId(b));
  // Similar is not the same: one letter, or a different publisher, is another book.
  Book c = a;
  c.title = "The Color of Magic";
  CHECK(library::stableId(a) != library::stableId(c));
  Book d = a;
  d.publisher = "Corgi";
  CHECK(library::stableId(a) != library::stableId(d));
}

// ---- the library the updates run against ---------------------------------------

std::vector<Book> libraryWith(const std::vector<bookbuddy::ImportedBook>& imported) {
  std::vector<Book> books;
  library::applyComplete(books, imported, {});
  return books;
}

std::vector<bookbuddy::ImportedBook> parsed(const std::vector<std::map<std::string, std::string>>& rows) {
  return bookbuddy::parse(exportOf(rows)).books;
}

// ---- quick update -------------------------------------------------------------

void testQuickUpdateAddsNewBooksToTheChosenCollection() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony()}));
  const library::UpdateReport preview = library::previewQuick(books, parsed({colourOfMagic(), perfume()}));
  CHECK(preview.found == 2 && preview.added == 2 && preview.refreshed == 0 && preview.suppressed == 0);

  library::UpdateReport done = library::applyQuick(books, parsed({colourOfMagic()}), Collection::Wishlist);
  CHECK(done.added == 1 && done.refreshed == 0);
  int i = indexOf(books, "The Colour of Magic");
  CHECK(i >= 0 && books[i].collection == Collection::Wishlist);
  // Default personal state, apart from what BookBuddy carried.
  CHECK(i >= 0 && books[i].state == ReadState::Unread && books[i].rating == 0 && !books[i].favourite &&
        books[i].tags.empty() && !books[i].deleted);

  done = library::applyQuick(books, parsed({perfume()}), Collection::MyBooks);
  i = indexOf(books, "Il profumo");
  CHECK(done.added == 1);
  CHECK(i >= 0 && books[i].collection == Collection::MyBooks);
  // Added to My Books does not mean read; BookBuddy said Read, so it starts read.
  CHECK(i >= 0 && books[i].state == ReadState::Read);
  CHECK(done.myBooks == 2 && done.wishlist == 1);
}

void testQuickUpdateOfAKnownBookRefreshesDetailsAndKeepsTheRest() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony()}));
  Book& have = books[0];
  have.collection = Collection::Wishlist;  // moved by hand
  have.state = ReadState::Read;
  have.rating = 2;
  have.favourite = false;
  have.tags = "japan; stories";
  have.notes = "Lent by Anna.";
  have.location = "Bedroom shelf 2";

  std::map<std::string, std::string> again = lifeCeremony();
  again["Publisher"] = "Granta";  // BookBuddy corrected a detail
  again["Rating"] = "5.000000";   // and still says five stars and favourite
  const library::UpdateReport preview = library::previewQuick(books, parsed({again}));
  CHECK(preview.added == 0 && preview.refreshed == 1);
  const library::UpdateReport done = library::applyQuick(books, parsed({again}), Collection::MyBooks);
  CHECK(done.added == 0 && done.refreshed == 1);
  CHECK(books.size() == 1);  // no duplicate
  CHECK(books[0].publisher == "Granta");
  // The destination chosen for the quick update did not move a book already here.
  CHECK(books[0].collection == Collection::Wishlist);
  CHECK(books[0].state == ReadState::Read);
  CHECK(books[0].rating == 2);
  CHECK(!books[0].favourite);
  CHECK(books[0].tags == "japan; stories");
  CHECK(books[0].notes == "Lent by Anna.");
  CHECK(books[0].location == "Bedroom shelf 2");
}

void testQuickUpdateLeavesADeletedBookDeleted() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  books[1].deleted = true;
  const library::UpdateReport preview = library::previewQuick(books, parsed({colourOfMagic()}));
  CHECK(preview.suppressed == 1 && preview.added == 0);
  const library::UpdateReport done = library::applyQuick(books, parsed({colourOfMagic()}), Collection::MyBooks);
  CHECK(done.suppressed == 1 && done.added == 0 && done.refreshed == 0);
  CHECK(books.size() == 2);
  CHECK(books[1].deleted);
  library::Filter all;
  CHECK(library::select(books, all).size() == 1);
}

// ---- complete update ----------------------------------------------------------

void testCompleteUpdateImportsBothExportsIntoTheirCollections() {
  std::vector<Book> books;
  const library::UpdateReport done =
      library::applyComplete(books, parsed({lifeCeremony(), colourOfMagic()}), parsed({perfume()}));
  CHECK(done.added == 3 && done.refreshed == 0 && done.conflicts == 0);
  CHECK(done.myBooks == 2 && done.wishlist == 1);
  const int p = indexOf(books, "Il profumo");
  CHECK(p >= 0 && books[p].collection == Collection::Wishlist);
  const int l = indexOf(books, "Life Ceremony");
  // A new book starts with what BookBuddy said about it.
  CHECK(l >= 0 && books[l].collection == Collection::MyBooks && books[l].favourite && books[l].rating == 5);
}

void testCompleteUpdateRefreshesDetailsAndKeepsEveryPersonalField() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  Book& magic = books[indexOf(books, "The Colour of Magic")];
  magic.state = ReadState::Read;
  magic.rating = 4;
  magic.favourite = true;
  magic.tags = "discworld; comfort";
  magic.notes = "First one.";
  magic.location = "Living room bookcase";
  magic.collection = Collection::Wishlist;  // moved by hand, and still in BookBuddy's library export
  Book& life = books[indexOf(books, "Life Ceremony")];
  life.deleted = true;

  std::map<std::string, std::string> magicAgain = colourOfMagic();
  magicAgain["Year Published"] = "1983";
  magicAgain["Summary"] = "In a distant and second-hand set of dimensions...";
  magicAgain["Rating"] = "0.000000";
  magicAgain["Favorites"] = "0";
  magicAgain["Status"] = "Unread";
  magicAgain["Physical Location"] = "Somewhere else";
  const library::UpdateReport done =
      library::applyComplete(books, parsed({lifeCeremony(), magicAgain}), parsed({perfume()}));

  CHECK(books.size() == 3);
  const Book& m = books[indexOf(books, "The Colour of Magic")];
  CHECK(m.year == "1983");
  CHECK(m.plot == "In a distant and second-hand set of dimensions...");
  CHECK(m.state == ReadState::Read);
  CHECK(m.rating == 4);
  CHECK(m.favourite);
  CHECK(m.tags == "discworld; comfort");
  CHECK(m.notes == "First one.");
  CHECK(m.location == "Living room bookcase");
  CHECK(m.collection == Collection::Wishlist);
  CHECK(done.movesPreserved == 1);
  // Deleted in the library, still in the export: still deleted.
  CHECK(books[indexOf(books, "Life Ceremony")].deleted);
  CHECK(done.suppressed == 1);
  CHECK(done.added == 1 && done.refreshed == 1);
  CHECK(done.myBooks == 0 && done.wishlist == 2);
}

void testABookTheExportsForgotIsKept() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  books[indexOf(books, "The Colour of Magic")].tags = "kept";
  // The next export has only Life Ceremony.
  const library::UpdateReport done = library::applyComplete(books, parsed({lifeCeremony()}), {});
  CHECK(books.size() == 2);
  const int i = indexOf(books, "The Colour of Magic");
  CHECK(i >= 0 && !books[i].deleted && books[i].tags == "kept" && books[i].publisher == "Random House");
  CHECK(done.added == 0 && done.refreshed == 1 && done.myBooks == 2);
}

void testABookInBothExportsIsOneBook() {
  std::vector<Book> books;
  library::UpdateReport done = library::applyComplete(books, parsed({perfume()}), parsed({perfume()}));
  CHECK(books.size() == 1);
  CHECK(books[0].collection == Collection::MyBooks);
  CHECK(done.conflicts == 1 && done.added == 1);
  // With personal state already saying Wishlist, that stands.
  books[0].collection = Collection::Wishlist;
  done = library::applyComplete(books, parsed({perfume()}), parsed({perfume()}));
  CHECK(books.size() == 1 && books[0].collection == Collection::Wishlist);
  CHECK(done.conflicts == 1 && done.movesPreserved == 1);
}

void testAnEmptyOrBrokenExportChangesNothing() {
  // The parser refuses what is not an export, and the caller is told; the
  // library it would have updated is untouched because nothing was applied.
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  const std::vector<Book> before = books;
  const bookbuddy::ParseResult broken = bookbuddy::parse("garbage\n");
  CHECK(broken.invalid);
  const bookbuddy::ParseResult empty = bookbuddy::parse(std::string(kHeader) + "\n");
  CHECK(!empty.invalid && empty.books.empty());
  // Applied anyway, an empty export deletes nothing: absence is not deletion.
  const library::UpdateReport done = library::applyComplete(books, empty.books, empty.books);
  CHECK(books.size() == before.size());
  CHECK(done.added == 0 && done.myBooks == 2);
  for (size_t i = 0; i < books.size(); ++i) CHECK(books[i].title == before[i].title && !books[i].deleted);
}

// ---- the database file --------------------------------------------------------

void testDatabaseRoundTrip() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  books[0].plot += "\nA second paragraph\twith a tab and a \\ backslash.";
  books[1].collection = Collection::Wishlist;
  const std::string text = library::encodeDatabase(books);
  CHECK(text.rfind("# CrossPlay MY LIBRARY database v1\n", 0) == 0);
  std::vector<Book> back;
  CHECK(library::decodeDatabase(text, back));
  CHECK(back.size() == 2);
  if (back.size() != 2) return;
  for (size_t i = 0; i < 2; ++i) {
    CHECK(back[i].title == books[i].title);
    CHECK(back[i].author == books[i].author);
    CHECK(back[i].isbn == books[i].isbn);
    CHECK(back[i].publisher == books[i].publisher);
    CHECK(back[i].year == books[i].year);
    CHECK(back[i].genre == books[i].genre);
    CHECK(back[i].pages == books[i].pages);
    CHECK(back[i].language == books[i].language);
    CHECK(back[i].plot == books[i].plot);
    CHECK(back[i].collection == books[i].collection);
    CHECK(library::stableId(back[i]) == library::stableId(books[i]));
  }
  // The personal layer is not in this file: a book comes back with defaults,
  // for the personal file to fill.
  CHECK(!back[0].favourite && back[0].rating == 0);
  // Not a database: refused; a damaged line: skipped.
  CHECK(!library::decodeDatabase("Title\tAuthor\n", back));
  std::string damaged = library::encodeDatabase(books);
  damaged += "half a line\n";
  CHECK(library::decodeDatabase(damaged, back) && back.size() == 2);
}

// ---- the backup ---------------------------------------------------------------

void testBackupHoldsBothLayersOfEveryBook() {
  std::vector<Book> books = libraryWith(parsed({lifeCeremony(), colourOfMagic()}));
  books[1].collection = Collection::Wishlist;
  books[1].state = ReadState::Read;
  books[1].rating = 4;
  books[1].tags = "discworld";
  books[1].notes = "A note\twith a tab";
  books[1].deleted = true;
  const std::string text = library::encodeBackup(books);
  // A header row a spreadsheet can read, then one row a book, the deleted one
  // included and marked.
  CHECK(text.rfind(std::string(library::kBackupColumns) + "\n", 0) == 0);
  size_t rows = 0;
  for (const char c : text) rows += c == '\n';
  CHECK(rows == 3);
  const size_t at = text.find("The Colour of Magic");
  CHECK(at != std::string::npos);
  const size_t start = text.rfind('\n', at) + 1;
  const std::string row = text.substr(start, text.find('\n', at) - start);
  CHECK(row.find("\twishlist\t1\t") != std::string::npos);            // collection, deleted
  CHECK(row.find("\tRead\t4\t0\tdiscworld\t") != std::string::npos);  // state, rating, favourite, tags
  CHECK(row.find("A note\\twith a tab") != std::string::npos);        // the tab escaped, so still one row
  CHECK(row.find("9780552166591") != std::string::npos);
}

}  // namespace

int main() {
  testARealRowBecomesTheBookItDescribes();
  testEmptyOptionalColumnsAndTheYearFromTheDate();
  testAccentedTextSurvivesAndReadIsRead();
  testALongSummaryAndBothLineEndings();
  testMalformedInputFailsSafely();
  testQuickFileNames();
  testIdentityIsExactIsbnWhateverItsFormatting();
  testFallbackIdentityIsDeterministicAndNotFuzzy();
  testQuickUpdateAddsNewBooksToTheChosenCollection();
  testQuickUpdateOfAKnownBookRefreshesDetailsAndKeepsTheRest();
  testQuickUpdateLeavesADeletedBookDeleted();
  testCompleteUpdateImportsBothExportsIntoTheirCollections();
  testCompleteUpdateRefreshesDetailsAndKeepsEveryPersonalField();
  testABookTheExportsForgotIsKept();
  testABookInBothExportsIsOneBook();
  testAnEmptyOrBrokenExportChangesNothing();
  testDatabaseRoundTrip();
  testBackupHoldsBothLayersOfEveryBook();
  std::printf("mylibrary import: %d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
