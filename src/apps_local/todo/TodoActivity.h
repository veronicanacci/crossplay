#pragma once

// TO DO on the device. The thin layer: renderer, input, shelf, card, keyboard.
//
// Two views in one activity, the lists and one list, because the second is a
// page of the first and Back walks up. Every edit goes through todo::Store,
// which says whether anything changed, and the file is written when something
// did and not otherwise.

#include <functional>
#include <memory>
#include <string>

#include "../../activities/Activity.h"
#include "../../components/OptionPopup.h"
#include "../ui/ToyboxScreen.h"
#include "TodoCore.h"

class TodoActivity final : public Activity {
 public:
  TodoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("To Do", renderer, mappedInput) {}
  ~TodoActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View { Lists, Items };

  void load();
  void save();

  // The result of a tap, once the table has said which control it was.
  void handle(const freeink::ui::ActionEvent& event);
  // The "..." menu for a list: pin or unpin, mark complete or incomplete,
  // delete. The existing OptionPopup, which is what every menu of this shape
  // on the device already is.
  void showMenu(int list);
  // The existing keyboard, pushed for one line of text. `onText` runs with the
  // typed text when the user confirms; a cancel runs nothing, so nothing is
  // ever created from a cancel.
  void openKeyboard(const char* title, std::function<void(const std::string&)> onText);
  void page(int delta);

  View view = View::Lists;
  // The open list, as an index into the store. Only meaningful in View::Items.
  int openList = -1;
  int listsPage = 0;
  int itemsPage = 0;
  // Measured by render() from the same layout the screens draw, and read by
  // loop() to page. Both start at one page of one row until the first paint.
  int rowsOnPage = 1;
  int pageCount = 1;

  todo::Store store;
  OptionPopup menu;

  // The list name as the band shows it: capitals, the way every band in this
  // fork shouts, in a buffer render() owns because the model borrows it.
  char shoutedTitle[todo::kMaxTextChars * 4 + 1] = {};

  toybox::Interactions interactions;
  // Closed across the window where render() has cleared the table and not yet
  // finished refilling it; see ClippyFactsActivity for the reasoning.
  bool interactionsReady = false;
};
