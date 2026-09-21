#pragma once

// MY LIBRARY on the device. The thin layer: renderer, input, shelf, keyboard.
//
// One activity, a stack of views: home, a collection, a group list, a book
// list, one book. Back pops the stack; an empty stack is the shelf's turn.
// Milestone 1 keeps the library in memory and seeds it from sampleBooks().

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../../components/OptionPopup.h"
#include "../ui/ToyboxScreen.h"
#include "LibraryCore.h"

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
  enum class Kind { Home, Collection, Groups, Books, Detail, Search };

  struct View {
    Kind kind = Kind::Home;
    library::Filter filter;    // the collection, and for Groups/Books the browse and its value
    int page = 0;              // menu or list page; the plot's first line on a Detail
    int book = -1;             // the library index, on a Detail
    bool plotSection = false;  // on a Detail: SUMMARY rather than INFO below the title block
    int pageCount = 1;         // counted by render(), read by loop() to page
  };

  // What a popup asked for, done in loop() once the popup has closed: a popup's
  // callback runs inside the popup, and opening another one from there would
  // replace the callback while it is still running.
  enum class Pending { None, MainMenu, EditMenu, RatingMenu, ConfirmDelete, EditTags, EditNotes };

  View& top() { return stack.back(); }
  void push(const View& view);
  void handle(const freeink::ui::ActionEvent& event);
  void page(int delta);
  void openPending();
  void openKeyboard(const char* title, const std::string& initial, size_t maxChars,
                    std::function<void(const std::string&)> onText);

  // The rows of the menu the top view shows, in order; a Collection or Groups
  // view. `value` is what a tap on row i does.
  struct Row {
    const char* label;
    library::Browse browse;
  };
  std::vector<Row> collectionRows() const;

  void renderMenu(toybox::Screen& screen, View& view);
  void renderBooks(toybox::Screen& screen, View& view);
  void renderDetail(toybox::Screen& screen, View& view);

  std::vector<library::Book> books;
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

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
