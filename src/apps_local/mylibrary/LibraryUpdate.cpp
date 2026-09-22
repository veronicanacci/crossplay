#include "LibraryUpdate.h"

namespace library {

namespace {

int indexOf(const std::vector<Book>& books, const std::string& id) {
  for (size_t i = 0; i < books.size(); ++i) {
    if (stableId(books[i]) == id) return static_cast<int>(i);
  }
  return -1;
}

void count(const std::vector<Book>& books, UpdateReport& report) {
  report.myBooks = 0;
  report.wishlist = 0;
  for (const Book& book : books) {
    if (book.deleted) continue;
    if (book.collection == Collection::Wishlist) {
      ++report.wishlist;
    } else {
      ++report.myBooks;
    }
  }
}

// A book the library has never seen: BookBuddy's bibliographic fields, the
// collection the caller chose, and the personal values BookBuddy carried, since
// there is no device state yet for them to lose to.
Book newBook(const bookbuddy::ImportedBook& imported, const Collection collection) {
  Book book = imported.meta;
  book.collection = collection;
  book.favourite = imported.favourite;
  book.rating = clampRating(imported.rating);
  book.state = imported.state;
  book.location = imported.location;
  book.tags = imported.tags;
  book.notes = imported.notes;
  book.deleted = false;
  return book;
}

// One imported book against the library. Returns true when it was new.
bool merge(std::vector<Book>& books, const bookbuddy::ImportedBook& imported, const Collection collection,
           UpdateReport& report) {
  const int index = indexOf(books, stableId(imported.meta));
  if (index < 0) {
    books.push_back(newBook(imported, collection));
    ++report.added;
    return true;
  }
  Book& have = books[index];
  refreshMetadata(imported, have);
  if (have.deleted) {
    ++report.suppressed;
  } else {
    ++report.refreshed;
    if (have.collection != collection) ++report.movesPreserved;
  }
  return false;
}

}  // namespace

void refreshMetadata(const bookbuddy::ImportedBook& from, Book& onto) {
  const Book& meta = from.meta;
  if (!meta.title.empty()) onto.title = meta.title;
  if (!meta.author.empty()) onto.author = meta.author;
  if (!meta.isbn.empty()) onto.isbn = meta.isbn;
  if (!meta.publisher.empty()) onto.publisher = meta.publisher;
  if (!meta.year.empty()) onto.year = meta.year;
  if (!meta.genre.empty()) onto.genre = meta.genre;
  if (!meta.series.empty()) onto.series = meta.series;
  if (!meta.pages.empty()) onto.pages = meta.pages;
  if (!meta.language.empty()) onto.language = meta.language;
  if (!meta.plot.empty()) onto.plot = meta.plot;
}

UpdateReport previewQuick(const std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& imported) {
  UpdateReport report;
  report.found = static_cast<int>(imported.size());
  for (const bookbuddy::ImportedBook& book : imported) {
    const int index = indexOf(books, stableId(book.meta));
    if (index < 0) {
      ++report.added;
    } else if (books[index].deleted) {
      ++report.suppressed;
    } else {
      ++report.refreshed;
    }
  }
  count(books, report);
  return report;
}

UpdateReport applyQuick(std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& imported,
                        const Collection destination) {
  UpdateReport report;
  report.found = static_cast<int>(imported.size());
  for (const bookbuddy::ImportedBook& book : imported) merge(books, book, destination, report);
  // A quick update names a destination for NEW books; a book already here was
  // never going to move, so that is not a preserved move worth reporting.
  report.movesPreserved = 0;
  count(books, report);
  return report;
}

UpdateReport applyComplete(std::vector<Book>& books, const std::vector<bookbuddy::ImportedBook>& fromLibrary,
                           const std::vector<bookbuddy::ImportedBook>& fromWishlist) {
  UpdateReport report;
  report.found = static_cast<int>(fromLibrary.size() + fromWishlist.size());
  // The library export first, so a book in both lands in My Books: the more
  // useful mistake, since a book you own and wanted is a book you own.
  std::vector<std::string> seen;
  seen.reserve(fromLibrary.size());
  for (const bookbuddy::ImportedBook& book : fromLibrary) {
    merge(books, book, Collection::MyBooks, report);
    seen.push_back(stableId(book.meta));
  }
  for (const bookbuddy::ImportedBook& book : fromWishlist) {
    const std::string id = stableId(book.meta);
    bool duplicate = false;
    for (const std::string& s : seen) duplicate = duplicate || s == id;
    if (duplicate) {
      ++report.conflicts;
      continue;
    }
    merge(books, book, Collection::Wishlist, report);
  }
  count(books, report);
  return report;
}

// ---- the database file --------------------------------------------------------

namespace {

constexpr char kDatabaseHeader[] = "# CrossPlay MY LIBRARY database v1";
constexpr char kDatabaseColumns[] =
    "id\tisbn\ttitle\tauthor\tpublisher\tyear\tgenre\tseries\tpages\tlanguage\tcollection\tsummary";

std::string escaped(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == '\\') {
      out += "\\\\";
    } else if (c == '\t') {
      out += "\\t";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c != '\r') {
      out.push_back(c);
    }
  }
  return out;
}

std::string unescaped(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\\' || i + 1 >= text.size()) {
      out.push_back(text[i]);
      continue;
    }
    const char next = text[++i];
    out.push_back(next == 't' ? '\t' : (next == 'n' ? '\n' : next));
  }
  return out;
}

std::vector<std::string> splitTabs(const std::string& line) {
  std::vector<std::string> fields;
  size_t start = 0;
  while (start <= line.size()) {
    size_t end = line.find('\t', start);
    if (end == std::string::npos) end = line.size();
    fields.push_back(line.substr(start, end - start));
    start = end + 1;
  }
  return fields;
}

}  // namespace

std::string encodeDatabase(const std::vector<Book>& books) {
  std::string out = kDatabaseHeader;
  out.push_back('\n');
  out += kDatabaseColumns;
  out.push_back('\n');
  for (const Book& book : books) {
    const std::string* fields[] = {&book.isbn,  &book.title,  &book.author, &book.publisher, &book.year,
                                   &book.genre, &book.series, &book.pages,  &book.language};
    out += escaped(stableId(book));
    for (const std::string* f : fields) {
      out.push_back('\t');
      out += escaped(*f);
    }
    out.push_back('\t');
    out += book.collection == Collection::Wishlist ? "wishlist" : "mybooks";
    out.push_back('\t');
    out += escaped(book.plot);
    out.push_back('\n');
  }
  return out;
}

bool decodeDatabase(const std::string& text, std::vector<Book>& out) {
  out.clear();
  if (text.compare(0, sizeof(kDatabaseHeader) - 1, kDatabaseHeader) != 0) return false;
  size_t start = 0;
  bool first = true;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    if (first) {
      // The column line. Not read: the columns are this file's own.
      first = false;
      continue;
    }
    const std::vector<std::string> f = splitTabs(line);
    if (f.size() < 11) continue;
    Book book;
    book.isbn = unescaped(f[1]);
    book.title = unescaped(f[2]);
    book.author = unescaped(f[3]);
    book.publisher = unescaped(f[4]);
    book.year = unescaped(f[5]);
    book.genre = unescaped(f[6]);
    book.series = unescaped(f[7]);
    book.pages = unescaped(f[8]);
    book.language = unescaped(f[9]);
    book.collection = f[10] == "wishlist" ? Collection::Wishlist : Collection::MyBooks;
    if (f.size() > 11) book.plot = unescaped(f[11]);
    if (book.title.empty() && book.isbn.empty()) continue;
    out.push_back(book);
  }
  return true;
}

// ---- the backup ---------------------------------------------------------------

const char* const kBackupColumns =
    "id\tcollection\tdeleted\ttitle\tauthor\tisbn\tpublisher\tyear\tgenre\tseries\tpages\tlanguage\tread_state\t"
    "rating\tfavourite\ttags\tlocation\tnotes\tsummary";

std::string encodeBackup(const std::vector<Book>& books) {
  std::string out = kBackupColumns;
  out.push_back('\n');
  for (const Book& book : books) {
    out += escaped(stableId(book));
    out.push_back('\t');
    out += book.collection == Collection::Wishlist ? "wishlist" : "mybooks";
    out.push_back('\t');
    out += book.deleted ? "1" : "0";
    const std::string* text[] = {&book.title, &book.author, &book.isbn,  &book.publisher, &book.year,
                                 &book.genre, &book.series, &book.pages, &book.language};
    for (const std::string* f : text) {
      out.push_back('\t');
      out += escaped(*f);
    }
    out.push_back('\t');
    out += readStateText(book.state);
    out.push_back('\t');
    out.push_back(static_cast<char>('0' + clampRating(book.rating)));
    out.push_back('\t');
    out += book.favourite ? "1" : "0";
    const std::string* personal[] = {&book.tags, &book.location, &book.notes, &book.plot};
    for (const std::string* f : personal) {
      out.push_back('\t');
      out += escaped(*f);
    }
    out.push_back('\n');
  }
  return out;
}

}  // namespace library
