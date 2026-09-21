#include "LibraryCore.h"

#include <algorithm>
#include <cctype>

namespace library {

bool isGrouped(const Browse browse) {
  return browse == Browse::Authors || browse == Browse::Genres || browse == Browse::Tags || browse == Browse::Series;
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

std::string lowered(const std::string& text) {
  std::string out = text;
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
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
    case Browse::All:
    default:
      return "ALL BOOKS";
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
  for (const std::string& t : splitTags(book.tags)) {
    if (t == tag) return true;
  }
  return false;
}

}  // namespace

bool matches(const Book& book, const Filter& filter) {
  if (book.deleted || book.collection != filter.collection) return false;
  switch (filter.browse) {
    case Browse::All:
      return true;
    case Browse::Authors:
      return book.author == filter.value;
    case Browse::Genres:
      return book.genre == filter.value;
    case Browse::Tags:
      return hasTag(book, filter.value);
    case Browse::Series:
      return book.series == filter.value;
    case Browse::Read:
      return book.state == ReadState::Read;
    case Browse::Unread:
      return book.state == ReadState::Unread;
    case Browse::Favourites:
      return book.favourite;
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
                   [&books](const int a, const int b) { return lowered(books[a].title) < lowered(books[b].title); });
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
      if (std::find(out.begin(), out.end(), value) == out.end()) out.push_back(value);
    }
  }
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lowered(a) < lowered(b); });
  return out;
}

int pageStep(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  const int from = page < 0 ? 0 : (page >= pageCount ? pageCount - 1 : page);
  const int to = from + delta;
  if (to < 0) return 0;
  return to >= pageCount ? pageCount - 1 : to;
}

}  // namespace library
