#include "LibraryActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <variant>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxTheme.h"
#include "LibraryScreens.h"

namespace fui = freeink::ui;

namespace {

constexpr const char* kLog = "LIBR";

constexpr size_t kTagChars = 120;
constexpr size_t kNoteChars = 200;
constexpr size_t kLocationChars = 60;
constexpr size_t kQueryChars = 48;

// The home screen's rows, and the update screen's.
constexpr const char* kHomeRows[] = {"MY BOOKS", "WISHLIST", "UPDATE LIBRARY"};
constexpr const char* kUpdateRows[] = {"QUICK UPDATE", "COMPLETE UPDATE", "BACKUP"};
constexpr const char* kBackupTitle = "BACKUP";
constexpr const char* kImportPill = "IMPORT";
constexpr const char* kUpdatePill = "UPDATE";
constexpr const char* kQuickTitle = "QUICK UPDATE";
constexpr const char* kCompleteTitle = "COMPLETE UPDATE";
constexpr const char* kDoneTitle = "UPDATED";
constexpr const char* kUnreadable = "Could not read BookBuddy export.";
// The search screen's three rows, and the mode each one searches.
constexpr const char* kSearchRows[] = {"TITLE", "AUTHOR", "ISBN"};
constexpr library::Browse kSearchModes[] = {library::Browse::SearchTitle, library::Browse::SearchAuthor,
                                            library::Browse::SearchIsbn};

// A whole file into a string, in 4 KB reads. Storage.readFile() stops at
// 50 KB and reads a byte at a time, which was thirty-five books of a
// 420 KB export on the device (the simulator's card has no such cap, so it
// read all of them and nothing said). Reserved from the file's size, so a
// large file is one allocation, which on the S3 lands in PSRAM.
bool readWhole(const char* path, std::string& out) {
  out.clear();
  HalFile file;
  if (!Storage.openFileForRead(kLog, path, file)) return false;
  out.reserve(file.fileSize());
  char chunk[4096];
  for (;;) {
    const int n = file.read(chunk, sizeof(chunk));
    if (n <= 0) break;
    out.append(chunk, static_cast<size_t>(n));
  }
  return true;
}

}  // namespace

std::unique_ptr<Activity> LibraryActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Never a bare new: the firmware is built -fno-exceptions, so a failed
  // allocation aborts rather than throwing.
  return makeUniqueNoThrow<LibraryActivity>(renderer, mappedInput);
}

void LibraryActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // The tree the updates read from and write to, made so the folders are
  // there to drop files into before the first update ever runs.
  for (const char* dir : {library::kLibraryDir, library::kImportsDir, library::kFullDir, library::kQuickDir,
                          library::kProcessedDir, library::kDatabaseDir, library::kStateDir}) {
    Storage.ensureDirectoryExists(dir);
  }
  books.clear();
  if (Storage.exists(library::kDatabasePath)) {
    std::string text;
    if (!readWhole(library::kDatabasePath, text) || !library::decodeDatabase(text, books, false)) {
      LOG_ERR(kLog, "%s is not a library database; starting from the fixture", library::kDatabasePath);
      books.clear();
    } else {
      LOG_INF(kLog, "%d books from %s", static_cast<int>(books.size()), library::kDatabasePath);
    }
  }
  // Before the first complete update there is no database, and the fixture
  // stands in for one so the screens have something to show.
  fromFixture = books.empty();
  if (fromFixture) books = library::sampleBooks();
  load();
  stack.clear();
  stack.push_back(View{});
  requestUpdate();
}

void LibraryActivity::load() {
  others.clear();
  // The file moved under /library/ with the database; one written at the old
  // path is read from there once and rewritten at the new one.
  const char* path = library::kPersonalPath;
  bool migrate = false;
  if (!Storage.exists(path)) {
    if (!Storage.exists(library::kLegacyPersonalPath)) {
      LOG_INF(kLog, "No %s; every book starts as its source has it", path);
      return;
    }
    path = library::kLegacyPersonalPath;
    migrate = true;
  }
  std::string text;
  if (!readWhole(path, text)) {
    LOG_ERR(kLog, "%s would not open; every book starts as its source has it", path);
    return;
  }
  std::vector<library::PersonalRecord> records;
  library::decodePersonal(text, records);
  library::loadPersonal(records, books, others);
  LOG_INF(kLog, "%d personal records from %s, %d for books not here", static_cast<int>(records.size()), path,
          static_cast<int>(others.size()));
  if (migrate) save();
}

void LibraryActivity::save() {
  const std::string text = library::encodePersonal(books, others);
  if (!Storage.writeFile(library::kPersonalPath, String(text.c_str()))) {
    LOG_ERR(kLog, "Could not write %s; personal state is only in RAM until the next save", library::kPersonalPath);
  }
}

const std::string& LibraryActivity::plotOf(const library::Book& book) {
  if (!book.plot.empty()) return book.plot;
  const std::string id = library::stableId(book);
  if (id == plotCacheId) return plotCache;
  plotCacheId = id;
  plotCache.clear();
  std::string text;
  if (!readWhole(library::kDatabasePath, text)) return plotCache;
  std::vector<library::Book> one;
  one.push_back(book);
  library::restorePlots(text, one);
  plotCache = one[0].plot;
  return plotCache;
}

void LibraryActivity::restorePlots(std::vector<library::Book>& into) {
  std::string text;
  if (Storage.exists(library::kDatabasePath) && readWhole(library::kDatabasePath, text)) {
    library::restorePlots(text, into);
  }
}

void LibraryActivity::dropPlots() {
  for (library::Book& book : books) book.plot.clear();
  plotCacheId.clear();
  plotCache.clear();
}

bool LibraryActivity::saveDatabase() {
  // Whole file to a temporary name, then the rename, so the old library is
  // on the card until the new one is all there.
  const std::string temp = std::string(library::kDatabasePath) + ".tmp";
  const std::string text = library::encodeDatabase(books);
  if (!Storage.writeFile(temp.c_str(), String(text.c_str()))) {
    LOG_ERR(kLog, "Could not write %s", temp.c_str());
    return false;
  }
  if (Storage.exists(library::kDatabasePath)) Storage.remove(library::kDatabasePath);
  if (!Storage.rename(temp.c_str(), library::kDatabasePath)) {
    LOG_ERR(kLog, "Could not rename %s into place", temp.c_str());
    return false;
  }
  return true;
}

bool LibraryActivity::readExport(const std::string& path, bookbuddy::ParseResult& out) {
  if (!Storage.exists(path.c_str())) return false;
  std::string text;
  if (!readWhole(path.c_str(), text)) {
    out.invalid = true;
    out.error = "The file would not open";
    LOG_ERR(kLog, "%s would not open", path.c_str());
    return false;
  }
  out = bookbuddy::parse(text);
  if (out.invalid) LOG_ERR(kLog, "%s: %s", path.c_str(), out.error.c_str());
  return !out.invalid;
}

void LibraryActivity::scanQuick() {
  quickFiles.clear();
  for (const String& name : Storage.listFiles(library::kQuickDir)) {
    const std::string file(name.c_str());
    if (!bookbuddy::isTsvName(file)) continue;
    QuickFile entry;
    entry.name = file;
    bookbuddy::ParseResult parsed;
    if (readExport(std::string(library::kQuickDir) + "/" + file, parsed)) {
      char count[toybox::kIntChars + 8];
      std::snprintf(count, sizeof(count), parsed.books.size() == 1 ? "%d book" : "%d books",
                    static_cast<int>(parsed.books.size()));
      entry.count = count;
      entry.readable = !parsed.books.empty();
    } else {
      entry.count = "unreadable";
    }
    quickFiles.push_back(entry);
  }
  std::sort(quickFiles.begin(), quickFiles.end(),
            [](const QuickFile& a, const QuickFile& b) { return library::fold(a.name) < library::fold(b.name); });
}

void LibraryActivity::showReport(const char* title, const std::string& text, const Confirm confirm) {
  View report;
  report.kind = Kind::Report;
  report.title = title;
  report.text = text;
  report.confirm = confirm;
  push(report);
}

namespace {

std::string counted(const int n, const char* one, const char* many) {
  char line[toybox::kIntChars + 64];
  std::snprintf(line, sizeof(line), "%d %s", n, n == 1 ? one : many);
  return line;
}

std::string totals(const library::UpdateReport& report) {
  char line[2 * toybox::kIntChars + 32];
  std::snprintf(line, sizeof(line), "My Books: %d\nWishlist: %d\n", report.myBooks, report.wishlist);
  return line;
}

}  // namespace

void LibraryActivity::quickPreview(const int file, const library::Collection destination) {
  if (file < 0 || file >= static_cast<int>(quickFiles.size())) return;
  bookbuddy::ParseResult parsed;
  const std::string name = quickFiles[file].name;
  if (!readExport(std::string(library::kQuickDir) + "/" + name, parsed) || parsed.books.empty()) {
    showReport(kQuickTitle, name + "\n\n" + kUnreadable + "\n" + parsed.error);
    return;
  }
  const library::UpdateReport what = library::previewQuick(books, parsed.books);
  std::string text = name + "\n" + counted(what.found, "book found", "books found") + "\n\n";
  text += counted(what.added, "new", "new") + "\n";
  text += counted(what.refreshed, "already in library", "already in library") + "\n";
  if (what.suppressed > 0)
    text += counted(what.suppressed, "deleted book stays deleted", "deleted books stay deleted") + "\n";
  if (parsed.skipped > 0) text += counted(parsed.skipped, "line skipped", "lines skipped") + "\n";
  text += "\nAdd to: ";
  text += destination == library::Collection::Wishlist ? "Wishlist" : "My Books";
  text += "\n\nBooks already here keep their collection and personal data; only their details are refreshed.";
  View report;
  report.kind = Kind::Report;
  report.title = kQuickTitle;
  report.text = text;
  report.confirm = Confirm::Quick;
  report.file = file;
  report.destination = destination;
  push(report);
}

void LibraryActivity::quickImport(View& view) {
  if (view.file < 0 || view.file >= static_cast<int>(quickFiles.size())) return;
  const std::string name = quickFiles[view.file].name;
  const std::string path = std::string(library::kQuickDir) + "/" + name;
  bookbuddy::ParseResult parsed;
  if (!readExport(path, parsed) || parsed.books.empty()) {
    view.title = kQuickTitle;
    view.text = name + "\n\n" + kUnreadable;
    view.confirm = Confirm::None;
    return;
  }
  // Into a copy, and the copy becomes the library only once it is on the card.
  // The fixture is a placeholder, not a library: the first update starts from
  // nothing rather than carrying nine sample books into the real one.
  std::vector<library::Book> next = fromFixture ? std::vector<library::Book>() : books;
  const library::UpdateReport done = library::applyQuick(next, parsed.books, view.destination);
  restorePlots(next);
  books.swap(next);
  if (!saveDatabase()) {
    books.swap(next);
    view.title = kQuickTitle;
    view.text = "Could not write the library file.\nNothing was changed.";
    view.confirm = Confirm::None;
    return;
  }
  fromFixture = false;
  dropPlots();
  save();
  // Out of the way of the next scan, name kept, so the file is still there to
  // look at. A failed move only means it is offered again.
  const std::string moved = std::string(library::kProcessedDir) + "/" + name;
  if (Storage.exists(moved.c_str())) Storage.remove(moved.c_str());
  if (!Storage.rename(path.c_str(), moved.c_str())) LOG_ERR(kLog, "Could not move %s to processed", path.c_str());
  std::string text = counted(done.added, "book added", "books added") + "\n";
  text += counted(done.refreshed, "record refreshed", "records refreshed") + "\n";
  if (done.suppressed > 0) text += counted(done.suppressed, "deleted book ignored", "deleted books ignored") + "\n";
  text += "0 duplicates created\n\n" + totals(done);
  view.title = kDoneTitle;
  view.text = text;
  view.confirm = Confirm::None;
  scanQuick();
}

void LibraryActivity::completePreview() {
  const std::string libraryPath = std::string(library::kFullDir) + "/" + library::kFullLibraryFile;
  const std::string wishlistPath = std::string(library::kFullDir) + "/" + library::kFullWishlistFile;
  std::string missing;
  if (!Storage.exists(libraryPath.c_str())) missing += "Missing " + libraryPath + "\n";
  if (!Storage.exists(wishlistPath.c_str())) missing += "Missing " + wishlistPath + "\n";
  if (!missing.empty()) {
    showReport(kCompleteTitle, missing + "\nBoth full exports are needed. Nothing was changed.");
    return;
  }
  bookbuddy::ParseResult mine;
  bookbuddy::ParseResult wish;
  if (!readExport(libraryPath, mine) || !readExport(wishlistPath, wish)) {
    showReport(kCompleteTitle, std::string(kUnreadable) + "\n" + mine.error + wish.error + "\nNothing was changed.");
    return;
  }
  // A full export with no books in it is not a library with no books in it;
  // it is a broken file, and it wipes nothing.
  if (mine.books.empty() && wish.books.empty()) {
    showReport(kCompleteTitle, "The exports hold no books.\nNothing was changed.");
    return;
  }
  std::string text = std::string(library::kFullLibraryFile) + ": " +
                     counted(static_cast<int>(mine.books.size()), "book", "books") + "\n";
  text += std::string(library::kFullWishlistFile) + ": " +
          counted(static_cast<int>(wish.books.size()), "book", "books") + "\n";
  if (mine.skipped + wish.skipped > 0)
    text += counted(mine.skipped + wish.skipped, "line skipped", "lines skipped") + "\n";
  text +=
      "\nNew books join the collection their export names. Books already here keep their read state, rating, "
      "favourite, tags, notes, location and collection. Deleted books stay deleted. Books the exports no longer "
      "mention are kept.";
  showReport(kCompleteTitle, text, Confirm::Complete);
}

void LibraryActivity::completeImport(View& view) {
  const std::string libraryPath = std::string(library::kFullDir) + "/" + library::kFullLibraryFile;
  const std::string wishlistPath = std::string(library::kFullDir) + "/" + library::kFullWishlistFile;
  bookbuddy::ParseResult mine;
  bookbuddy::ParseResult wish;
  view.title = kCompleteTitle;
  view.confirm = Confirm::None;
  if (!readExport(libraryPath, mine) || !readExport(wishlistPath, wish) || (mine.books.empty() && wish.books.empty())) {
    view.text = std::string(kUnreadable) + "\nNothing was changed.";
    return;
  }
  // The fixture is a placeholder, not a library: the first update starts from
  // nothing rather than carrying nine sample books into the real one.
  std::vector<library::Book> next = fromFixture ? std::vector<library::Book>() : books;
  const library::UpdateReport done = library::applyComplete(next, mine.books, wish.books);
  restorePlots(next);
  books.swap(next);
  if (!saveDatabase()) {
    books.swap(next);
    view.text = "Could not write the library file.\nNothing was changed.";
    return;
  }
  fromFixture = false;
  dropPlots();
  save();
  std::string text = totals(done) + "\n";
  text += counted(done.added, "new book", "new books") + "\n";
  text += counted(done.refreshed, "metadata update", "metadata updates") + "\n";
  text += counted(done.movesPreserved, "local move preserved", "local moves preserved") + "\n";
  text += counted(done.suppressed, "deleted book suppressed", "deleted books suppressed") + "\n";
  if (done.conflicts > 0)
    text +=
        counted(done.conflicts, "book in both exports, kept in My Books", "books in both exports, kept in My Books") +
        "\n";
  text += "\nUpdate complete.";
  view.title = kDoneTitle;
  view.text = text;
}

void LibraryActivity::backup() {
  Storage.ensureDirectoryExists(library::kBackupDir);
  // Named by the clock when the device has one set; a clock still at the
  // epoch names the file by count instead, so two backups never share a name.
  char name[64];
  const time_t now = time(nullptr);
  if (now > 1600000000) {
    struct tm parts;
    localtime_r(&now, &parts);
    std::snprintf(name, sizeof(name), "backup-%04d%02d%02d-%02d%02d%02d.tsv", parts.tm_year + 1900, parts.tm_mon + 1,
                  parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec);
  } else {
    int n = 1;
    do {
      std::snprintf(name, sizeof(name), "backup-%04d.tsv", n++);
    } while (Storage.exists((std::string(library::kBackupDir) + "/" + name).c_str()) && n < 10000);
  }
  const std::string path = std::string(library::kBackupDir) + "/" + name;
  std::vector<library::Book> whole = books;
  restorePlots(whole);
  const std::string text = library::encodeBackup(whole);
  if (!Storage.writeFile(path.c_str(), String(text.c_str()))) {
    LOG_ERR(kLog, "Could not write %s", path.c_str());
    showReport(kBackupTitle, "Could not write\n" + path + "\n\nNothing was saved.");
    return;
  }
  library::UpdateReport tally;
  int deleted = 0;
  for (const library::Book& book : books) {
    if (book.deleted) {
      ++deleted;
    } else if (book.collection == library::Collection::Wishlist) {
      ++tally.wishlist;
    } else {
      ++tally.myBooks;
    }
  }
  std::string report = "Saved\n" + path + "\n\n" + totals(tally);
  report += counted(deleted, "deleted book kept for the record", "deleted books kept for the record");
  report +=
      "\n\nEvery book with its details and your read state, rating, favourite, tags, notes, location and collection, "
      "as one sheet.";
  showReport(kBackupTitle, report);
}

void LibraryActivity::search(const library::Browse browse) {
  const library::Collection collection = top().filter.collection;
  char title[32];
  std::snprintf(title, sizeof(title), "Search %s", library::browseTitle(browse));
  openKeyboard(title, std::string(), kQueryChars, [this, browse, collection](const std::string& text) {
    // Nothing typed is nothing searched: the keyboard closes and the search
    // screen is still there.
    if (library::fold(text).find_first_not_of(' ') == std::string::npos) return;
    View next;
    next.kind = Kind::Books;
    next.filter.collection = collection;
    next.filter.browse = browse;
    next.filter.value = text;
    push(next);
  });
}

void LibraryActivity::push(const View& view) {
  stack.push_back(view);
  requestUpdate();
}

std::vector<LibraryActivity::Row> LibraryActivity::collectionRows() const {
  using library::Browse;
  const bool owned = stack.back().filter.collection == library::Collection::MyBooks;
  std::vector<Row> rows = {
      {"SEARCH", Browse::All},    {"ALL BOOKS", Browse::All}, {"AUTHORS", Browse::Authors},
      {"GENRES", Browse::Genres}, {"TAGS", Browse::Tags},     {"SERIES", Browse::Series},
  };
  if (owned) {
    rows.push_back({"READ", Browse::Read});
    rows.push_back({"UNREAD", Browse::Unread});
  }
  rows.push_back({"FAVOURITES", Browse::Favourites});
  return rows;
}

void LibraryActivity::page(const int delta) {
  View& view = top();
  const int next = library::pageStep(view.page, view.pageCount, delta);
  if (next == view.page) return;
  view.page = next;
  requestUpdate();
}

void LibraryActivity::loop() {
  // The menu owns every input while it is up, including Back, which closes it.
  if (menu.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (pending != Pending::None) {
    openPending();
    return;
  }

  // Back walks up the stack; off the bottom of it, wherever the shelf says.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (stack.size() > 1) {
      stack.pop_back();
      if (top().kind == Kind::QuickFiles) scanQuick();
      requestUpdate();
    } else {
      shelf::leave(renderer, mappedInput);
    }
    return;
  }

  // The two side keys and a vertical swipe page the rows, the way the shelf's
  // folders do.
  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  const bool next =
      mappedInput.wasReleased(MappedInputManager::Button::Down) || swipe == MappedInputManager::SwipeDir::Up;
  const bool prev =
      mappedInput.wasReleased(MappedInputManager::Button::Up) || swipe == MappedInputManager::SwipeDir::Down;
  if (next || prev) {
    page(next ? 1 : -1);
    return;
  }

  int tapX = 0, tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  if (!interactionsReady) return;

  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions.route(input);
  if (!event) return;
  handle(event);
}

void LibraryActivity::handle(const freeink::ui::ActionEvent& event) {
  View& view = top();
  switch (event.action) {
    case libraryui::ActionRow: {
      const int row = event.value;
      if (view.kind == Kind::Home) {
        if (row < 0 || row > 2) return;
        View next;
        if (row == 2) {
          next.kind = Kind::Update;
        } else {
          next.kind = Kind::Collection;
          next.filter.collection = row == 0 ? library::Collection::MyBooks : library::Collection::Wishlist;
        }
        push(next);
      } else if (view.kind == Kind::Update) {
        if (row == 0) {
          scanQuick();
          View next;
          next.kind = Kind::QuickFiles;
          push(next);
        } else if (row == 1) {
          completePreview();
        } else if (row == 2) {
          backup();
        }
      } else if (view.kind == Kind::QuickFiles) {
        if (row < 0 || row >= static_cast<int>(quickFiles.size())) return;
        if (!quickFiles[row].readable) {
          showReport(kQuickTitle, quickFiles[row].name + "\n\n" + kUnreadable);
          return;
        }
        pendingFile = row;
        pending = Pending::QuickDestination;
      } else if (view.kind == Kind::Collection) {
        const std::vector<Row> rows = collectionRows();
        if (row < 0 || row >= static_cast<int>(rows.size())) return;
        View next;
        next.filter = view.filter;
        next.filter.browse = rows[row].browse;
        if (row == 0) {
          next.kind = Kind::Search;
        } else {
          next.kind = library::isGrouped(rows[row].browse) ? Kind::Groups : Kind::Books;
        }
        push(next);
      } else if (view.kind == Kind::Search) {
        if (row < 0 || row > 2) return;
        search(kSearchModes[row]);
      } else if (view.kind == Kind::Groups) {
        if (row < 0 || row >= static_cast<int>(groupNames.size())) return;
        View next;
        next.kind = Kind::Books;
        next.filter = view.filter;
        next.filter.value = groupNames[row];
        push(next);
      }
      return;
    }

    case libraryui::ActionOpenBook: {
      if (event.value < 0 || event.value >= static_cast<int>(books.size())) return;
      View next;
      next.kind = Kind::Detail;
      next.filter = view.filter;
      next.book = event.value;
      push(next);
      return;
    }

    case libraryui::ActionConfirm:
      if (view.kind != Kind::Report) return;
      if (view.confirm == Confirm::Quick) {
        quickImport(view);
      } else if (view.confirm == Confirm::Complete) {
        completeImport(view);
      }
      requestUpdate();
      return;

    case libraryui::ActionSwitchSection:
      if (view.kind != Kind::Detail) return;
      // Two sections, so either arrow lands on the other; the summary starts
      // from its first page each time it is opened.
      view.plotSection = !view.plotSection;
      view.page = 0;
      requestUpdate();
      return;

    case libraryui::ActionHeart:
      if (view.kind != Kind::Detail) return;
      books[view.book].favourite = !books[view.book].favourite;
      save();
      requestUpdate();
      return;

    case libraryui::ActionMore:
      if (view.kind != Kind::Detail) return;
      pending = Pending::MainMenu;
      return;

    default:
      return;
  }
}

void LibraryActivity::openPending() {
  const Pending what = pending;
  pending = Pending::None;
  if (what == Pending::QuickDestination) {
    // Where the NEW books go. Mandatory, and the user's to say: BookBuddy's
    // own wishlist flag is not consulted, and a book already here keeps the
    // collection it has whatever is chosen.
    const char* options[2] = {"My Books", "Wishlist"};
    menu.show("ADD TO", options, 2, 0, [this](const int choice) {
      pendingDestination = choice == 1 ? library::Collection::Wishlist : library::Collection::MyBooks;
      pending = Pending::QuickPreview;
      requestUpdate();
    });
    requestUpdate();
    return;
  }
  if (what == Pending::QuickPreview) {
    quickPreview(pendingFile, pendingDestination);
    return;
  }
  if (top().kind != Kind::Detail) return;
  const int index = top().book;
  library::Book& book = books[index];
  const bool owned = book.collection == library::Collection::MyBooks;

  switch (what) {
    case Pending::MainMenu: {
      const char* options[3] = {"Edit personal data", owned ? "Move to Wishlist" : "Move to My Books", "Delete Book"};
      menu.show(book.title.c_str(), options, 3, 0, [this, index, owned](const int choice) {
        if (choice == 0) {
          pending = Pending::EditMenu;
        } else if (choice == 1) {
          // Only the collection changes: every other personal field stays.
          books[index].collection = owned ? library::Collection::Wishlist : library::Collection::MyBooks;
          save();
        } else if (choice == 2) {
          pending = Pending::ConfirmDelete;
        }
        requestUpdate();
      });
      break;
    }

    case Pending::EditMenu: {
      std::vector<std::string> options;
      if (owned) {
        // The two states the book is not in. "Mark as" what it already is
        // would be a row that does nothing.
        if (book.state != library::ReadState::Read) options.push_back("Mark as read");
        if (book.state != library::ReadState::Unread) options.push_back("Mark as unread");
        if (book.state != library::ReadState::Dnf) options.push_back("Mark as DNF");
        options.push_back(std::string("Rating: ") + library::ratingText(book.rating));
      }
      options.push_back(book.favourite ? "Remove from favourites" : "Add to favourites");
      options.push_back("Tags");
      options.push_back("Notes");
      if (owned) options.push_back("Location");
      // Rows after the two My Books-only ones shift in the wishlist, so a choice
      // is read back by its label rather than its number.
      const std::vector<std::string> labels = options;
      std::vector<const char*> pointers;
      pointers.reserve(options.size());
      for (const std::string& option : options) pointers.push_back(option.c_str());
      menu.show("Edit personal data", pointers.data(), static_cast<int>(pointers.size()), 0,
                [this, index, labels](const int choice) {
                  if (choice < 0 || choice >= static_cast<int>(labels.size())) return;
                  const std::string& label = labels[choice];
                  library::Book& b = books[index];
                  if (label == "Mark as read") {
                    b.state = library::ReadState::Read;
                  } else if (label == "Mark as unread") {
                    b.state = library::ReadState::Unread;
                  } else if (label == "Mark as DNF") {
                    b.state = library::ReadState::Dnf;
                  } else if (label.rfind("Rating", 0) == 0) {
                    pending = Pending::RatingMenu;
                  } else if (label.find("favourites") != std::string::npos) {
                    b.favourite = !b.favourite;
                  } else if (label == "Tags") {
                    pending = Pending::EditTags;
                  } else if (label == "Notes") {
                    pending = Pending::EditNotes;
                  } else if (label == "Location") {
                    pending = Pending::EditLocation;
                  }
                  save();
                  requestUpdate();
                });
      break;
    }

    case Pending::EditTags:
      openKeyboard("Tags (semicolon separated)", book.tags, kTagChars, [this, index](const std::string& text) {
        books[index].tags = text;
        save();
      });
      return;
    case Pending::EditNotes:
      openKeyboard("Notes", book.notes, kNoteChars, [this, index](const std::string& text) {
        books[index].notes = text;
        save();
      });
      return;
    case Pending::EditLocation:
      openKeyboard("Location", book.location, kLocationChars, [this, index](const std::string& text) {
        books[index].location = text;
        save();
      });
      return;

    case Pending::RatingMenu: {
      const char* options[6] = {library::ratingText(0), library::ratingText(1), library::ratingText(2),
                                library::ratingText(3), library::ratingText(4), library::ratingText(5)};
      menu.show("Rating", options, 6, library::clampRating(book.rating), [this, index](const int choice) {
        books[index].rating = library::clampRating(choice);
        save();
        requestUpdate();
      });
      break;
    }

    case Pending::ConfirmDelete: {
      const char* options[2] = {"Delete", "Keep"};
      menu.show(book.title.c_str(), options, 2, 1, [this, index](const int choice) {
        if (choice == 0) {
          // A tombstone, not an erasure: the book stays in the table so a later
          // import cannot bring it back.
          books[index].deleted = true;
          save();
          if (stack.size() > 1) stack.pop_back();
        }
        requestUpdate();
      });
      break;
    }

    case Pending::None:
      break;
  }
  requestUpdate();
}

void LibraryActivity::openKeyboard(const char* title, const std::string& initial, const size_t maxChars,
                                   std::function<void(const std::string&)> onText) {
  // The device's one keyboard, the one every settings field uses.
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, std::string(title), initial,
                                                                 maxChars, InputType::Text),
                         [onText](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           const auto* typed = std::get_if<KeyboardResult>(&result.data);
                           if (typed == nullptr || typed->headerAction) return;
                           onText(typed->text);
                         });
}

// ---- rendering ----------------------------------------------------------------

void LibraryActivity::renderMenu(toybox::Screen& screen, View& view) {
  items.clear();
  groupNames.clear();
  groupCounts.clear();

  libraryui::MenuModel model;
  if (view.kind == Kind::Home) {
    model.title = libraryui::kTitle;
    for (const char* label : kHomeRows) {
      fui::ListItem item;
      item.label = label;
      items.push_back(item);
    }
  } else if (view.kind == Kind::Update) {
    model.title = "UPDATE LIBRARY";
    for (const char* label : kUpdateRows) {
      fui::ListItem item;
      item.label = label;
      items.push_back(item);
    }
  } else if (view.kind == Kind::QuickFiles) {
    model.title = kQuickTitle;
    model.reading = true;
    model.empty = "No files in /library/imports/quick";
    for (const QuickFile& file : quickFiles) {
      fui::ListItem item;
      item.label = file.name.c_str();
      item.value = file.count.c_str();
      items.push_back(item);
    }
  } else if (view.kind == Kind::Search) {
    model.title = "SEARCH";
    for (const char* label : kSearchRows) {
      fui::ListItem item;
      item.label = label;
      items.push_back(item);
    }
  } else if (view.kind == Kind::Collection) {
    model.title = library::collectionTitle(view.filter.collection);
    for (const Row& row : collectionRows()) {
      fui::ListItem item;
      item.label = row.label;
      items.push_back(item);
    }
  } else {
    model.title = library::browseTitle(view.filter.browse);
    model.reading = true;
    model.empty = library::groupsEmptyText(view.filter.browse);
    groupNames = library::groups(books, view.filter.collection, view.filter.browse);
    groupCounts.reserve(groupNames.size());
    for (const std::string& name : groupNames) {
      library::Filter filter = view.filter;
      filter.value = name;
      char count[toybox::kIntTextChars];
      std::snprintf(count, sizeof(count), "%d", static_cast<int>(library::select(books, filter).size()));
      groupCounts.push_back(count);
    }
    for (size_t i = 0; i < groupNames.size(); ++i) {
      fui::ListItem item;
      item.label = groupNames[i].c_str();
      item.value = groupCounts[i].c_str();
      items.push_back(item);
    }
  }

  // Paged the way the shelf pages: a slice of rows, each carrying its index in
  // the whole menu.
  const int perPage = libraryui::menuRowsPerPage(screen.device(), screen.theme());
  const int total = static_cast<int>(items.size());
  view.pageCount = total > 0 ? (total + perPage - 1) / perPage : 1;
  view.page = library::pageStep(view.page, view.pageCount, 0);
  const int first = view.page * perPage;
  const int onPage = total - first < perPage ? total - first : perPage;
  for (int i = 0; i < total; ++i) items[i].actionValue = static_cast<int16_t>(i);

  model.items = onPage > 0 ? items.data() + first : nullptr;
  model.count = onPage > 0 ? onPage : 0;
  model.page = view.page;
  model.pageCount = view.pageCount;
  libraryui::buildMenu(screen, model);
}

void LibraryActivity::renderBooks(toybox::Screen& screen, View& view) {
  selected = library::select(books, view.filter);
  const int count = static_cast<int>(selected.size());
  const fui::Rect rows = libraryui::bookRows(screen.device());
  const int16_t width = libraryui::bookTextWidth(rows);
  heights.clear();
  heights.reserve(selected.size());
  for (const int index : selected) {
    heights.push_back(libraryui::bookRowHeight(screen.target(), screen.theme(), books[index].title.c_str(), width));
  }
  const std::vector<int> starts = libraryui::pageStarts(rows, heights.data(), count);
  view.pageCount = static_cast<int>(starts.size());
  view.page = library::pageStep(view.page, view.pageCount, 0);
  const int first = starts[view.page];
  const int last = view.page + 1 < view.pageCount ? starts[view.page + 1] : count;
  const int onPage = last - first > libraryui::kMaxRowsOnPage ? libraryui::kMaxRowsOnPage : last - first;

  libraryui::BookRow rowModels[libraryui::kMaxRowsOnPage];
  for (int i = 0; i < onPage; ++i) {
    const library::Book& book = books[selected[first + i]];
    rowModels[i].title = book.title.c_str();
    rowModels[i].author = book.author.c_str();
    rowModels[i].value = static_cast<int16_t>(selected[first + i]);
  }

  // The caption: the browse and, for a grouped one, the value picked.
  char caption[160];
  if (library::isGrouped(view.filter.browse) || library::isSearch(view.filter.browse)) {
    std::snprintf(caption, sizeof(caption), "%s: %s", library::browseTitle(view.filter.browse),
                  view.filter.value.c_str());
  } else if (count == 1) {
    std::snprintf(caption, sizeof(caption), "%s, 1 book", library::browseTitle(view.filter.browse));
  } else {
    std::snprintf(caption, sizeof(caption), "%s, %d books", library::browseTitle(view.filter.browse), count);
  }

  libraryui::BookListModel model;
  model.title = library::collectionTitle(view.filter.collection);
  model.caption = caption;
  model.rows = rowModels;
  model.count = onPage > 0 ? onPage : 0;
  model.empty = count == 0;
  model.emptyText = library::booksEmptyText(view.filter.browse);
  model.page = view.page;
  model.pageCount = view.pageCount;
  libraryui::buildBookList(screen, model);
}

void LibraryActivity::renderDetail(toybox::Screen& screen, View& view) {
  const library::Book& book = books[view.book];
  libraryui::DetailModel model;
  model.band = library::collectionTitle(book.collection);
  model.title = book.title.c_str();
  model.author = book.author.c_str();
  model.isbn = book.isbn.c_str();
  model.publisher = book.publisher.c_str();
  model.year = book.year.c_str();
  model.genre = book.genre.c_str();
  model.series = book.series.c_str();
  model.pages = book.pages.c_str();
  model.language = book.language.c_str();
  // Fetched from the card only when the section is open: the summary is the
  // one part of a book this activity does not keep in RAM.
  model.plot = view.plotSection ? plotOf(book).c_str() : "";
  model.showRead = book.collection == library::Collection::MyBooks;
  model.readState = library::readStateText(book.state);
  model.rating = library::ratingText(book.rating);
  model.favourite = book.favourite;
  model.tags = book.tags.c_str();
  model.notes = book.notes.c_str();
  model.showPlot = view.plotSection;

  // The plot pages by whole screens of lines; `page` holds the page number and
  // the builder pins it to what the plot really has.
  model.plotPage = view.page;
  const libraryui::DetailPaging paging = libraryui::buildDetail(screen, model);
  view.pageCount = paging.pageCount;
  view.page = paging.page;
}

void LibraryActivity::render(RenderLock&&) {
  // Drawn over the frame already on the panel, without clearing it, the way
  // every popup on the device is.
  if (menu.processRender(renderer, mappedInput)) return;

  View& view = top();
  if (view.kind == Kind::Detail && (view.book < 0 || view.book >= static_cast<int>(books.size()))) {
    stack.pop_back();
    return render(RenderLock{});
  }

  renderer.clearScreen();
  // Menus of the device's own words take Jersey, like the shelf. Anything that
  // is a book's words (names, titles, prose) takes the reading cuts: bold for
  // titles in the small slot, regular for everything else in the body slot.
  const bool jersey = view.kind == Kind::Home || view.kind == Kind::Collection || view.kind == Kind::Search ||
                      view.kind == Kind::Update;
  // A report is prose with a Jersey pill under it, which is readingChromeFaces.
  fui::GfxRendererTarget target =
      toybox::makeTarget(renderer, jersey                      ? toybox::toyboxFaces()
                                   : view.kind == Kind::Report ? toybox::readingChromeFaces()
                                                               : toybox::readingAddressFaces());
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};

  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);

  switch (view.kind) {
    case Kind::Home:
    case Kind::Collection:
    case Kind::Search:
    case Kind::Update:
    case Kind::QuickFiles:
    case Kind::Groups:
      renderMenu(screen, view);
      break;
    case Kind::Report: {
      libraryui::ReportModel model;
      model.title = view.title.c_str();
      model.text = view.text.c_str();
      model.action = view.confirm == Confirm::Quick      ? kImportPill
                     : view.confirm == Confirm::Complete ? kUpdatePill
                                                         : nullptr;
      libraryui::buildReport(screen, model);
      break;
    }
    case Kind::Books:
      renderBooks(screen, view);
      break;
    case Kind::Detail:
      renderDetail(screen, view);
      break;
  }
  interactionsReady = true;

  toybox::reportOverflow(interactions, "My Library");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
