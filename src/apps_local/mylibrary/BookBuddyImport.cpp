#include "BookBuddyImport.h"

#include <cctype>
#include <cstdlib>

namespace bookbuddy {

namespace {

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

std::string trimmed(const std::string& text) {
  size_t a = 0;
  size_t b = text.size();
  while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) --b;
  return text.substr(a, b - a);
}

// Where each column this parser reads sits in THIS file's header, or -1.
struct Columns {
  int title = -1, series = -1, author = -1, publisher = -1, datePublished = -1, yearPublished = -1, genre = -1,
      summary = -1, pages = -1, language = -1, isbn = -1, favorites = -1, rating = -1, location = -1, status = -1,
      tags = -1, notes = -1, wishlist = -1;
};

int indexOf(const std::vector<std::string>& header, const char* name) {
  for (size_t i = 0; i < header.size(); ++i) {
    if (trimmed(header[i]) == name) return static_cast<int>(i);
  }
  return -1;
}

Columns columnsOf(const std::vector<std::string>& header) {
  Columns c;
  c.title = indexOf(header, column::kTitle);
  c.series = indexOf(header, column::kSeries);
  c.author = indexOf(header, column::kAuthor);
  c.publisher = indexOf(header, column::kPublisher);
  c.datePublished = indexOf(header, column::kDatePublished);
  c.yearPublished = indexOf(header, column::kYearPublished);
  c.genre = indexOf(header, column::kGenre);
  c.summary = indexOf(header, column::kSummary);
  c.pages = indexOf(header, column::kPages);
  c.language = indexOf(header, column::kLanguage);
  c.isbn = indexOf(header, column::kIsbn);
  c.favorites = indexOf(header, column::kFavorites);
  c.rating = indexOf(header, column::kRating);
  c.location = indexOf(header, column::kLocation);
  c.status = indexOf(header, column::kStatus);
  c.tags = indexOf(header, column::kTags);
  c.notes = indexOf(header, column::kNotes);
  c.wishlist = indexOf(header, column::kWishList);
  return c;
}

std::string field(const std::vector<std::string>& row, const int index) {
  if (index < 0 || index >= static_cast<int>(row.size())) return std::string();
  return trimmed(row[index]);
}

// "2012" from Year Published, or the first four digits of Date Published
// ("2012/07/09") when the year column is empty.
std::string yearOf(const std::vector<std::string>& row, const Columns& c) {
  const std::string year = field(row, c.yearPublished);
  if (!year.empty()) return year;
  const std::string date = field(row, c.datePublished);
  if (date.size() >= 4) {
    bool digits = true;
    for (int i = 0; i < 4; ++i) digits = digits && std::isdigit(static_cast<unsigned char>(date[i]));
    if (digits) return date.substr(0, 4);
  }
  return std::string();
}

bool isTrue(const std::string& value) { return value == "1" || value == "true" || value == "TRUE" || value == "Yes"; }

}  // namespace

bool isTsvName(const std::string& name) {
  const size_t dot = name.rfind('.');
  if (dot == std::string::npos) return false;
  std::string ext = name.substr(dot);
  for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".tsv" || ext == ".txt";
}

ParseResult parse(const std::string& text) {
  ParseResult result;
  size_t start = 0;
  // A byte-order mark, should an editor have added one.
  if (text.size() >= 3 && text.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;

  size_t end = text.find('\n', start);
  if (end == std::string::npos) end = text.size();
  std::string headerLine = text.substr(start, end - start);
  if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
  const std::vector<std::string> header = splitTabs(headerLine);
  const Columns c = columnsOf(header);
  if (headerLine.empty() || header.size() < 2 || c.title < 0) {
    result.invalid = true;
    result.error = "Not a BookBuddy export: no Title column";
    return result;
  }
  const size_t width = header.size();
  start = end + 1;

  while (start < text.size()) {
    end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (trimmed(line).empty()) continue;
    const std::vector<std::string> row = splitTabs(line);
    // A line with the wrong number of fields is not a book: a tab or a newline
    // inside a field would shift every column after it, and a book read from
    // the wrong columns is worse than one skipped.
    if (row.size() != width) {
      ++result.skipped;
      continue;
    }
    ImportedBook book;
    book.meta.title = field(row, c.title);
    book.meta.isbn = field(row, c.isbn);
    if (book.meta.title.empty() && book.meta.isbn.empty()) {
      ++result.skipped;
      continue;
    }
    book.meta.author = field(row, c.author);
    book.meta.publisher = field(row, c.publisher);
    book.meta.year = yearOf(row, c);
    book.meta.genre = field(row, c.genre);
    book.meta.series = field(row, c.series);
    book.meta.pages = field(row, c.pages);
    book.meta.language = field(row, c.language);
    book.meta.plot = field(row, c.summary);

    book.favourite = isTrue(field(row, c.favorites));
    // "5.000000" rounds to 5; anything unparseable is 0.
    book.rating = library::clampRating(static_cast<int>(std::atof(field(row, c.rating).c_str()) + 0.5));
    book.location = field(row, c.location);
    const std::string status = field(row, c.status);
    book.state = (status == "Read" || status == "Finished") ? library::ReadState::Read : library::ReadState::Unread;
    book.tags = field(row, c.tags);
    book.notes = field(row, c.notes);
    book.wishlist = isTrue(field(row, c.wishlist));
    result.books.push_back(book);
  }
  return result;
}

}  // namespace bookbuddy
