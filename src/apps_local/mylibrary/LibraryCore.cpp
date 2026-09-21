#include "LibraryCore.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace library {

bool isGrouped(const Browse browse) {
  return browse == Browse::Authors || browse == Browse::Genres || browse == Browse::Tags || browse == Browse::Series;
}

bool isSearch(const Browse browse) {
  return browse == Browse::SearchTitle || browse == Browse::SearchAuthor || browse == Browse::SearchIsbn;
}

namespace {

Book make(const char* title, const char* author, const char* isbn, const char* publisher, const char* year,
          const char* genre, const char* series, const char* pages, const char* language, const char* plot) {
  Book b;
  b.title = title;
  b.author = author;
  b.isbn = isbn;
  b.publisher = publisher;
  b.year = year;
  b.genre = genre;
  b.series = series;
  b.pages = pages;
  b.language = language;
  b.plot = plot;
  return b;
}

}  // namespace

std::vector<Book> sampleBooks() {
  std::vector<Book> books;
  books.reserve(9);

  Book b =
      make("The Name of the Rose", "Umberto Eco", "9780151446473", "Harcourt", "1983", "Mystery", "", "536", "English",
           "In 1327 the Franciscan friar William of Baskerville and his novice Adso arrive at a wealthy Italian "
           "abbey where a series of deaths is unsettling the monks. The trail leads through a labyrinthine "
           "library and a forbidden book.");
  b.state = ReadState::Read;
  b.rating = 5;
  b.favourite = true;
  b.tags = "italy; monks; labyrinth";
  b.location = "Bedroom shelf 2";
  b.notes = "Reread every winter.";
  books.push_back(b);

  b = make("If on a winter's night a traveler", "Italo Calvino", "9780156439619", "Harcourt", "1981", "Fiction", "",
           "260", "English",
           "You are about to begin reading Italo Calvino's new novel. Ten incipits, each interrupted, and a reader who "
           "keeps trying to finish a book.");
  b.state = ReadState::Read;
  b.rating = 4;
  b.tags = "italy; metafiction";
  b.location = "Bedroom shelf 2";
  books.push_back(b);

  b = make("Invisible Cities", "Italo Calvino", "9780156453806", "Harcourt", "1974", "Fiction", "", "165", "English",
           "Marco Polo describes fifty-five cities to Kublai Khan, none of which exist.");
  b.tags = "italy";
  b.location = "Living room bookcase";
  books.push_back(b);

  b = make("Perfume", "Patrick Süskind", "9780140120837", "Penguin", "1986", "Horror", "", "263", "English",
           "Jean-Baptiste Grenouille is born with a perfect sense of smell and no scent of his own. In eighteenth "
           "century France he sets out to distil the one perfume that will make him loved.");
  b.state = ReadState::Read;
  b.rating = 3;
  b.tags = "gothic; weird little man";
  b.location = "Living room bookcase";
  books.push_back(b);

  b = make("Dracula", "Bram Stoker", "9780141439846", "Penguin Classics", "1897", "Horror", "", "488", "English", "");
  b.favourite = true;
  b.tags = "gothic; vampires";
  b.location = "Storage box";
  books.push_back(b);

  b = make("Dune", "Frank Herbert", "9780441172719", "Ace", "1965", "Science Fiction", "Dune", "604", "English",
           "Paul Atreides and his family take over the desert planet Arrakis, the only source of the spice melange.");
  b.tags = "desert; sand";
  b.location = "Storage box";
  books.push_back(b);

  b = make("Dune Messiah", "Frank Herbert", "9780441172696", "Ace", "1969", "Science Fiction", "Dune", "256", "English",
           "");
  b.state = ReadState::Dnf;
  b.location = "Storage box";
  books.push_back(b);

  // The wishlist: two books not yet owned.
  b = make("The Shadow of the Wind", "Carlos Ruiz Zafón", "9780143034902", "Penguin", "2001", "Mystery",
           "The Cemetery of Forgotten Books", "487", "English",
           "In post-war Barcelona a boy chooses a forgotten novel and finds someone is destroying every copy of its "
           "author's work.");
  b.collection = Collection::Wishlist;
  b.favourite = true;
  b.tags = "gothic; barcelona";
  books.push_back(b);

  b = make("Piranesi", "Susanna Clarke", "9781635575637", "Bloomsbury", "2020", "Fantasy", "", "245", "English",
           "Piranesi lives in the House, an infinite building of halls and statues washed by tides, and keeps a "
           "journal of what he finds there.");
  b.collection = Collection::Wishlist;
  b.tags = "labyrinth";
  books.push_back(b);

  return books;
}

const char* collectionTitle(const Collection collection) {
  return collection == Collection::Wishlist ? "WISHLIST" : "MY BOOKS";
}

const char* browseTitle(const Browse browse) {
  switch (browse) {
    case Browse::Authors:
      return "AUTHORS";
    case Browse::Genres:
      return "GENRES";
    case Browse::Tags:
      return "TAGS";
    case Browse::Series:
      return "SERIES";
    case Browse::Read:
      return "READ";
    case Browse::Unread:
      return "UNREAD";
    case Browse::Favourites:
      return "FAVOURITES";
    case Browse::SearchTitle:
      return "TITLE";
    case Browse::SearchAuthor:
      return "AUTHOR";
    case Browse::SearchIsbn:
      return "ISBN";
    case Browse::All:
    default:
      return "ALL BOOKS";
  }
}

const char* groupsEmptyText(const Browse browse) {
  switch (browse) {
    case Browse::Authors:
      return "No authors found.";
    case Browse::Genres:
      return "No genres found.";
    case Browse::Tags:
      return "No tags yet.";
    case Browse::Series:
      return "No series found.";
    default:
      return "Nothing here yet.";
  }
}

const char* booksEmptyText(const Browse browse) {
  switch (browse) {
    case Browse::Favourites:
      return "No favourites yet.";
    case Browse::Read:
      return "Nothing read yet.";
    case Browse::Unread:
      return "Nothing unread.";
    case Browse::SearchTitle:
    case Browse::SearchAuthor:
    case Browse::SearchIsbn:
      return "No matching books.";
    default:
      return "No books found.";
  }
}

int clampRating(const int rating) { return rating < 0 ? 0 : (rating > 5 ? 5 : rating); }

const char* ratingText(const int rating) {
  static const char* const kTexts[6] = {"Not rated", "*", "**", "***", "****", "*****"};
  return kTexts[clampRating(rating)];
}

const char* readStateText(const ReadState state) {
  switch (state) {
    case ReadState::Read:
      return "Read";
    case ReadState::Dnf:
      return "DNF";
    case ReadState::Unread:
    default:
      return "Unread";
  }
}

std::vector<std::string> splitTags(const std::string& text) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find(';', start);
    if (end == std::string::npos) end = text.size();
    size_t a = start;
    size_t b = end;
    while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
    if (b > a) out.push_back(text.substr(a, b - a));
    start = end + 1;
  }
  return out;
}

namespace {

bool hasTag(const Book& book, const std::string& tag) {
  const std::string wanted = fold(tag);
  for (const std::string& t : splitTags(book.tags)) {
    if (fold(t) == wanted) return true;
  }
  return false;
}

// A substring match on the folded text. An empty query matches nothing: a
// search for nothing is not a search for everything.
bool contains(const std::string& text, const std::string& query) {
  if (query.empty()) return false;
  return fold(text).find(fold(query)) != std::string::npos;
}

}  // namespace

bool matches(const Book& book, const Filter& filter) {
  if (book.deleted || book.collection != filter.collection) return false;
  switch (filter.browse) {
    case Browse::All:
      return true;
    case Browse::Authors:
      return fold(book.author) == fold(filter.value);
    case Browse::Genres:
      return fold(book.genre) == fold(filter.value);
    case Browse::Tags:
      return hasTag(book, filter.value);
    case Browse::Series:
      return fold(book.series) == fold(filter.value);
    case Browse::Read:
      return book.state == ReadState::Read;
    case Browse::Unread:
      return book.state == ReadState::Unread;
    case Browse::Favourites:
      return book.favourite;
    case Browse::SearchTitle:
      return contains(book.title, filter.value);
    case Browse::SearchAuthor:
      return contains(book.author, filter.value);
    case Browse::SearchIsbn: {
      const std::string query = foldIsbn(filter.value);
      return !query.empty() && foldIsbn(book.isbn).find(query) != std::string::npos;
    }
  }
  return false;
}

std::vector<int> select(const std::vector<Book>& books, const Filter& filter) {
  std::vector<int> out;
  out.reserve(books.size());
  for (size_t i = 0; i < books.size(); ++i) {
    if (matches(books[i], filter)) out.push_back(static_cast<int>(i));
  }
  std::stable_sort(out.begin(), out.end(),
                   [&books](const int a, const int b) { return fold(books[a].title) < fold(books[b].title); });
  return out;
}

std::vector<std::string> groups(const std::vector<Book>& books, const Collection collection, const Browse browse) {
  std::vector<std::string> out;
  for (const Book& book : books) {
    if (book.deleted || book.collection != collection) continue;
    std::vector<std::string> values;
    switch (browse) {
      case Browse::Authors:
        values.push_back(book.author);
        break;
      case Browse::Genres:
        values.push_back(book.genre);
        break;
      case Browse::Series:
        values.push_back(book.series);
        break;
      case Browse::Tags:
        values = splitTags(book.tags);
        break;
      default:
        break;
    }
    for (const std::string& value : values) {
      if (value.empty()) continue;
      // One entry per name however it is capitalised, spelt the way it was
      // first seen.
      const std::string key = fold(value);
      bool seen = false;
      for (const std::string& have : out) seen = seen || fold(have) == key;
      if (!seen) out.push_back(value);
    }
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const std::string& a, const std::string& b) { return fold(a) < fold(b); });
  return out;
}

int pageStep(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  const int from = page < 0 ? 0 : (page >= pageCount ? pageCount - 1 : page);
  const int to = from + delta;
  if (to < 0) return 0;
  return to >= pageCount ? pageCount - 1 : to;
}

// ---- normalisation ----------------------------------------------------------

namespace {

// The base letter for U+00C0..U+017F, one character per code point, in order.
// '1' stands for "ss" (U+00DF), '2' for "ae" (U+00C6/E6), '3' for "oe"
// (U+0152/53), '4' for "th" (U+00DE/FE), and '.' for a code point that is not a
// letter with a diacritic and is left alone.
constexpr char kLatin1[64] = {
    // U+00C0: À Á Â Ã Ä Å Æ Ç È É Ê Ë Ì Í Î Ï
    'a',
    'a',
    'a',
    'a',
    'a',
    'a',
    '2',
    'c',
    'e',
    'e',
    'e',
    'e',
    'i',
    'i',
    'i',
    'i',
    // U+00D0: Ð Ñ Ò Ó Ô Õ Ö × Ø Ù Ú Û Ü Ý Þ ß
    'd',
    'n',
    'o',
    'o',
    'o',
    'o',
    'o',
    '.',
    'o',
    'u',
    'u',
    'u',
    'u',
    'y',
    '4',
    '1',
    // U+00E0: à á â ã ä å æ ç è é ê ë ì í î ï
    'a',
    'a',
    'a',
    'a',
    'a',
    'a',
    '2',
    'c',
    'e',
    'e',
    'e',
    'e',
    'i',
    'i',
    'i',
    'i',
    // U+00F0: ð ñ ò ó ô õ ö ÷ ø ù ú û ü ý þ ÿ
    'd',
    'n',
    'o',
    'o',
    'o',
    'o',
    'o',
    '.',
    'o',
    'u',
    'u',
    'u',
    'u',
    'y',
    '4',
    'y',
};
// U+0100..U+017F, Latin Extended-A: upper and lower alternate.
constexpr char kLatinA[128] = {
    'a', 'a', 'a', 'a', 'a', 'a', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'c', 'd', 'd',  // U+0100
    'd', 'd', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'e', 'g', 'g', 'g', 'g',  // U+0110
    'g', 'g', 'g', 'g', 'h', 'h', 'h', 'h', 'i', 'i', 'i', 'i', 'i', 'i', 'i', 'i',  // U+0120
    'i', 'i', '.', '.', 'j', 'j', 'k', 'k', 'k', 'l', 'l', 'l', 'l', 'l', 'l', 'l',  // U+0130
    'l', 'l', 'l', 'n', 'n', 'n', 'n', 'n', 'n', 'n', '.', '.', 'o', 'o', 'o', 'o',  // U+0140
    'o', 'o', '3', '3', 'r', 'r', 'r', 'r', 'r', 'r', 's', 's', 's', 's', 's', 's',  // U+0150
    's', 's', 't', 't', 't', 't', 't', 't', 'u', 'u', 'u', 'u', 'u', 'u', 'u', 'u',  // U+0160
    'u', 'u', 'u', 'u', 'w', 'w', 'y', 'y', 'y', 'z', 'z', 'z', 'z', 'z', 'z', '.',  // U+0170
};

void appendFolded(std::string& out, const char base) {
  switch (base) {
    case '1':
      out += "ss";
      return;
    case '2':
      out += "ae";
      return;
    case '3':
      out += "oe";
      return;
    case '4':
      out += "th";
      return;
    default:
      out.push_back(base);
      return;
  }
}

}  // namespace

std::string fold(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x80) {
      out.push_back(static_cast<char>(std::tolower(c)));
      continue;
    }
    // A two-byte sequence in U+0080..U+07FF; the two tables cover U+00C0..U+017F.
    if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
      const unsigned char d = static_cast<unsigned char>(text[i + 1]);
      const uint32_t cp = (static_cast<uint32_t>(c & 0x1F) << 6) | (d & 0x3F);
      char base = '.';
      if (cp >= 0xC0 && cp < 0x100) base = kLatin1[cp - 0xC0];
      if (cp >= 0x100 && cp < 0x180) base = kLatinA[cp - 0x100];
      if (base != '.') {
        appendFolded(out, base);
        i += 1;
        continue;
      }
    }
    out.push_back(text[i]);
  }
  return out;
}

std::string foldIsbn(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    if (c == ' ' || c == '-') continue;
    out.push_back(c);
  }
  return out;
}

// ---- identity -----------------------------------------------------------------

std::string stableId(const Book& book) {
  const std::string isbn = foldIsbn(book.isbn);
  if (isbn.size() == 10 || isbn.size() == 13) return "isbn:" + isbn;
  // FNV-1a over the folded fields, separated so "ab" + "c" and "a" + "bc" differ.
  uint64_t h = 1469598103934665603ULL;
  const std::string* parts[4] = {&book.author, &book.title, &book.publisher, &book.year};
  for (const std::string* part : parts) {
    for (const char c : fold(*part)) {
      h ^= static_cast<unsigned char>(c);
      h *= 1099511628211ULL;
    }
    h ^= 0x1F;
    h *= 1099511628211ULL;
  }
  char hex[24];
  std::snprintf(hex, sizeof(hex), "meta:%08lx%08lx", static_cast<unsigned long>(h >> 32),
                static_cast<unsigned long>(h & 0xFFFFFFFFULL));
  return hex;
}

// ---- the personal state file ------------------------------------------------

namespace {

constexpr char kPersonalHeader[] = "# CrossPlay MY LIBRARY personal v1";

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
    if (next == 't') {
      out.push_back('\t');
    } else if (next == 'n') {
      out.push_back('\n');
    } else {
      out.push_back(next);
    }
  }
  return out;
}

const char* collectionWord(const Collection collection) {
  return collection == Collection::Wishlist ? "wishlist" : "mybooks";
}

const char* stateWord(const ReadState state) {
  return state == ReadState::Read ? "read" : (state == ReadState::Dnf ? "dnf" : "unread");
}

void appendRecord(std::string& out, const PersonalRecord& record) {
  out += escaped(record.id);
  out.push_back('\t');
  out += collectionWord(record.collection);
  out.push_back('\t');
  out += stateWord(record.state);
  out.push_back('\t');
  out.push_back(static_cast<char>('0' + clampRating(record.rating)));
  out.push_back('\t');
  out.push_back(record.favourite ? '1' : '0');
  out.push_back('\t');
  out.push_back(record.deleted ? '1' : '0');
  out.push_back('\t');
  out += escaped(record.tags);
  out.push_back('\t');
  out += escaped(record.location);
  out.push_back('\t');
  out += escaped(record.notes);
  out.push_back('\n');
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

PersonalRecord personalOf(const Book& book) {
  PersonalRecord record;
  record.id = stableId(book);
  record.collection = book.collection;
  record.state = book.state;
  record.rating = book.rating;
  record.favourite = book.favourite;
  record.deleted = book.deleted;
  record.tags = book.tags;
  record.location = book.location;
  record.notes = book.notes;
  return record;
}

void applyPersonal(const PersonalRecord& record, Book& book) {
  book.collection = record.collection;
  book.state = record.state;
  book.rating = clampRating(record.rating);
  book.favourite = record.favourite;
  book.deleted = record.deleted;
  book.tags = record.tags;
  book.location = record.location;
  book.notes = record.notes;
}

std::string encodePersonal(const std::vector<Book>& books, const std::vector<PersonalRecord>& others) {
  std::string out = kPersonalHeader;
  out.push_back('\n');
  for (const Book& book : books) appendRecord(out, personalOf(book));
  for (const PersonalRecord& record : others) appendRecord(out, record);
  return out;
}

void decodePersonal(const std::string& text, std::vector<PersonalRecord>& out) {
  out.clear();
  size_t start = 0;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string> fields = splitTabs(line);
    // Nine fields today; a shorter line from an older writer keeps what it has,
    // and a longer one from a newer writer loses only what this reader does not
    // know.
    if (fields.size() < 6 || fields[0].empty()) continue;
    PersonalRecord record;
    record.id = unescaped(fields[0]);
    record.collection = fields[1] == "wishlist" ? Collection::Wishlist : Collection::MyBooks;
    record.state = fields[2] == "read" ? ReadState::Read : (fields[2] == "dnf" ? ReadState::Dnf : ReadState::Unread);
    record.rating = clampRating(std::atoi(fields[3].c_str()));
    record.favourite = fields[4] == "1";
    record.deleted = fields[5] == "1";
    if (fields.size() > 6) record.tags = unescaped(fields[6]);
    if (fields.size() > 7) record.location = unescaped(fields[7]);
    if (fields.size() > 8) record.notes = unescaped(fields[8]);
    out.push_back(record);
  }
}

void loadPersonal(const std::vector<PersonalRecord>& records, std::vector<Book>& books,
                  std::vector<PersonalRecord>& others) {
  others.clear();
  for (const PersonalRecord& record : records) {
    bool placed = false;
    for (Book& book : books) {
      if (stableId(book) == record.id) {
        applyPersonal(record, book);
        placed = true;
        break;
      }
    }
    if (!placed) others.push_back(record);
  }
}

}  // namespace library
