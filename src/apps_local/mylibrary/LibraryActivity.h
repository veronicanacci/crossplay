#pragma once

// MY LIBRARY on the device. The thin layer: renderer, input, shelf, keyboard,
// the card.
//
// One activity, a stack of views: home, a collection, a group list, a book
// list, one book, and the update screens. Back pops the stack; an empty stack
// is the shelf's turn.
//
// Two layers on the card. The bibliographic one is /library/database/library.tsv,
// written by the updates and read on entry (before the first update, the books
// come from sampleBooks()). The personal one is /library/state/personal.tsv,
// written after every change to a personal field. An update never writes the
// personal file except to add records for new books.

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../../components/OptionPopup.h"
#include "../ui/ToyboxScreen.h"
#include "BookBuddyImport.h"
#include "LibraryCore.h"
#include "LibraryUpdate.h"

class LibraryActivity final : public Activity {
 public:
  LibraryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("My Library", renderer, mappedInput) {}
  ~LibraryActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Kind { Home, Collection, Groups, Books, Detail, Search, Update, QuickFiles, Report };
  // What the pill on a Report does.
  enum class Confirm { None, Quick, Complete };

  struct View {
    Kind kind = Kind::Home;
    library::Filter filter;    // the collection, and for Groups/Books the browse and its value
    int page = 0;              // menu or list page; the plot's page on a Detail
    int book = -1;             // the library index, on a Detail
    bool plotSection = false;  // on a Detail: SUMMARY rather than INFO below the title block
    int pageCount = 1;         // counted by render(), read by loop() to page
    // A Report: its words, and what the pill does. A quick update also carries
    // the file it is about and where its new books go.
    std::string title;
    std::string text;
    Confirm confirm = Confirm::None;
    int file = -1;
    library::Collection destination = library::Collection::MyBooks;
  };

  // What a popup asked for, done in loop() once the popup has closed: a popup's
  // callback runs inside the popup, and opening another one from there would
  // replace the callback while it is still running.
  enum class Pending {
    None,
    MainMenu,
    EditMenu,
    RatingMenu,
    ConfirmDelete,
    EditTags,
    EditNotes,
    EditLocation,
    QuickDestination,  // "ADD TO" for the quick file just tapped
    QuickPreview       // the destination was chosen: show what the import would do
  };

  // A file in the quick folder, as the list shows it.
  struct QuickFile {
    std::string name;
    std::string count;  // "3 books", or why it cannot be read
    bool readable = false;
  };

  View& top() { return stack.back(); }
  void load();
  void save();
  // The database file, written whole to a temporary name and renamed over the
  // old one, so a failure half way leaves the previous library intact.
  bool saveDatabase();
  void search(library::Browse browse);
  void push(const View& view);
  void handle(const freeink::ui::ActionEvent& event);
  void page(int delta);
  void openPending();
  void openKeyboard(const char* title, const std::string& initial, size_t maxChars,
                    std::function<void(const std::string&)> onText);

  // The update flows.
  void scanQuick();
  bool readExport(const std::string& path, bookbuddy::ParseResult& out);
  void quickPreview(int file, library::Collection destination);
  void quickImport(View& view);
  void completePreview();
  void completeImport(View& view);
  void showReport(const char* title, const std::string& text, Confirm confirm = Confirm::None);

  struct Row {
    const char* label;
    library::Browse browse;
  };
  std::vector<Row> collectionRows() const;

  void renderMenu(toybox::Screen& screen, View& view);
  void renderBooks(toybox::Screen& screen, View& view);
  void renderDetail(toybox::Screen& screen, View& view);

  std::vector<library::Book> books;
  // Records in the personal file for books this library does not have, kept
  // and written back so a book that comes back finds its state waiting.
  std::vector<library::PersonalRecord> others;
  std::vector<QuickFile> quickFiles;
  // True while `books` is sampleBooks() standing in for a database that does
  // not exist yet. The first update starts from an empty library, not from them.
  bool fromFixture = false;
  std::vector<View> stack;
  // Scratch the models borrow for the length of a build. Members rather than
  // locals because render() runs on an 8 KB stack.
  std::vector<std::string> groupNames;
  std::vector<std::string> groupCounts;
  std::vector<freeink::ui::ListItem> items;
  std::vector<int> selected;
  std::vector<int16_t> heights;

  OptionPopup menu;
  Pending pending = Pending::None;
  int pendingFile = -1;
  library::Collection pendingDestination = library::Collection::MyBooks;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
