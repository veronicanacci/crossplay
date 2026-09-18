#include "TodoActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <variant>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "TodoScreens.h"

namespace {

constexpr const char* kLog = "TODO";

}  // namespace

std::unique_ptr<Activity> TodoActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Never a bare new: the firmware is built -fno-exceptions, so a failed
  // allocation aborts rather than throwing.
  return makeUniqueNoThrow<TodoActivity>(renderer, mappedInput);
}

void TodoActivity::load() {
  store.lists.clear();
  if (!Storage.exists(todo::kSavePath)) {
    LOG_INF(kLog, "No %s; starting with no lists", todo::kSavePath);
    return;
  }
  const String text = Storage.readFile(todo::kSavePath);
  todo::decode(std::string(text.c_str()), store.lists);
  LOG_INF(kLog, "%d lists from %s", static_cast<int>(store.lists.size()), todo::kSavePath);
}

void TodoActivity::save() {
  const std::string text = todo::encode(store.lists);
  if (!Storage.writeFile(todo::kSavePath, String(text.c_str()))) {
    LOG_ERR(kLog, "Could not write %s; the lists are only in RAM until the next save", todo::kSavePath);
  }
}

void TodoActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  load();
  requestUpdate();
}

void TodoActivity::page(const int delta) {
  int& current = view == View::Lists ? listsPage : itemsPage;
  const int next = todo::pageStep(current, pageCount, delta);
  if (next == current) return;
  current = next;
  requestUpdate();
}

void TodoActivity::loop() {
  namespace fui = freeink::ui;

  // The menu owns every input while it is up, including Back, which closes it.
  if (menu.handleInput(mappedInput, [this] { requestUpdate(); })) return;

  // Back walks up: a list to the lists, the lists to wherever the shelf says.
  // leave() is the one place that knows which folder that is. On the X4 Pro
  // this is also the left-edge swipe, which MappedInputManager folds into the
  // same release.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == View::Items) {
      view = View::Lists;
      openList = -1;
      requestUpdate();
    } else {
      shelf::leave(renderer, mappedInput);
    }
    return;
  }

  // The two side keys and a vertical swipe page the rows, the way the shelf's
  // folders and the Hacker News front page do.
  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  const bool next = mappedInput.wasReleased(MappedInputManager::Button::Down) || swipe == MappedInputManager::SwipeDir::Up;
  const bool prev = mappedInput.wasReleased(MappedInputManager::Button::Up) || swipe == MappedInputManager::SwipeDir::Down;
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
  // route() gates itself on whether the panel has actually SHOWN this table, so
  // a finger still resting where a shelf row was cannot land on a list during
  // the refresh that replaced it.
  const fui::ActionEvent event = interactions.route(input);
  if (!event) return;
  handle(event);
}

void TodoActivity::handle(const freeink::ui::ActionEvent& event) {
  switch (event.action) {
    case todoui::ActionOpenList:
      if (!store.validList(event.value)) return;
      openList = event.value;
      view = View::Items;
      itemsPage = 0;
      requestUpdate();
      return;

    case todoui::ActionListMenu:
      if (!store.validList(event.value)) return;
      showMenu(event.value);
      requestUpdate();
      return;

    case todoui::ActionNewList:
      openKeyboard("New list", [this](const std::string& text) {
        const int added = store.addList(text);
        if (added < 0) return;
        save();
        // Land on the page the new list is on: it joins the end of the
        // unfinished lists, which may be below the fold.
        const std::vector<int> order = todo::displayOrder(store.lists);
        for (size_t i = 0; i < order.size(); ++i) {
          if (order[i] == added) listsPage = todo::pageFor(static_cast<int>(i), rowsOnPage);
        }
      });
      return;

    case todoui::ActionToggleItem:
      if (store.toggleItem(openList, event.value)) {
        save();
        requestUpdate();
      }
      return;

    case todoui::ActionAddItem:
      openKeyboard("Add item", [this](const std::string& text) {
        const int added = store.addItem(openList, text);
        if (added < 0) return;
        save();
        itemsPage = todo::pageFor(added, rowsOnPage);
      });
      return;

    default:
      return;
  }
}

void TodoActivity::showMenu(const int list) {
  const todo::List& target = store.lists[list];
  const bool complete = todo::isComplete(target);
  const char* options[3] = {
      target.pinned ? "Unpin" : "Pin to top",
      complete ? "Mark incomplete" : "Mark complete",
      "Delete",
  };
  // Titled with the list's name, so a menu opened on the wrong row says so
  // before anything is done to it.
  menu.show(target.name.c_str(), options, 3, 0, [this, list, complete](const int choice) {
    if (!store.validList(list)) return;
    bool changed = false;
    switch (choice) {
      case 0:
        changed = store.setPinned(list, !store.lists[list].pinned);
        break;
      case 1:
        changed = store.markAll(list, !complete);
        break;
      case 2:
        changed = store.removeList(list);
        break;
      default:
        break;
    }
    if (changed) save();
    requestUpdate();
  });
}

void TodoActivity::openKeyboard(const char* title, std::function<void(const std::string&)> onText) {
  // The device's one keyboard, the one every settings field and the OPDS search
  // use, for one line of text. It pops itself with the text, or with
  // isCancelled set, and the handler below runs on this activity once it has.
  startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, std::string(title),
                                                                 std::string(), todo::kMaxTextChars, InputType::Text),
                         [onText](const ActivityResult& result) {
                           if (result.isCancelled) return;
                           // get_if, not get: the firmware is built without
                           // exceptions, and a result of another shape is a
                           // cancel rather than a crash.
                           const auto* typed = std::get_if<KeyboardResult>(&result.data);
                           if (typed == nullptr || typed->headerAction) return;
                           onText(typed->text);
                         });
}

void TodoActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  // Drawn over the frame already on the panel, without clearing it, the way
  // every popup on the device is.
  if (menu.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();
  // readingChromeFaces(): names and items are text somebody typed, so they take
  // the reading cut, which carries the accents Jersey does not; counts,
  // buttons and the status line are the device speaking and keep Jersey; the
  // band keeps the shared display cut so this is the same device as the shelf.
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::readingChromeFaces());
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};

  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);

  if (view == View::Items && !store.validList(openList)) {
    // The list went away under us (deleted from a menu opened before the view
    // changed, or a store reloaded shorter). Up a level rather than a blank.
    view = View::Lists;
    openList = -1;
  }

  if (view == View::Lists) {
    const todoui::Layout box = todoui::layout(device, false);
    rowsOnPage = todoui::rowsPerPage(box.rows, todoui::kListRowHeight);
    const std::vector<int> order = todo::displayOrder(store.lists);
    const int count = static_cast<int>(order.size());
    pageCount = todo::pageCountFor(count, rowsOnPage);
    listsPage = todo::pageStep(listsPage, pageCount, 0);
    const int first = listsPage * rowsOnPage;
    const int onPage = count - first < rowsOnPage ? count - first : rowsOnPage;

    todoui::ListRow rows[todoui::kMaxRowsOnPage];
    for (int i = 0; i < onPage; ++i) {
      const int index = order[first + i];
      const todo::List& list = store.lists[index];
      rows[i].name = list.name.c_str();
      rows[i].total = static_cast<int>(list.items.size());
      rows[i].open = todo::openCount(list);
      rows[i].pinned = list.pinned;
      rows[i].complete = todo::isComplete(list);
      rows[i].value = static_cast<int16_t>(index);
    }
    todoui::ListsModel model;
    model.rows = rows;
    model.count = onPage > 0 ? onPage : 0;
    model.page = listsPage;
    model.pageCount = pageCount;
    todoui::buildLists(screen, model);
  } else {
    const todo::List& list = store.lists[openList];
    const todoui::Layout box = todoui::layout(device, true);
    rowsOnPage = todoui::rowsPerPage(box.rows, todoui::kItemRowHeight);
    const int count = static_cast<int>(list.items.size());
    pageCount = todo::pageCountFor(count, rowsOnPage);
    itemsPage = todo::pageStep(itemsPage, pageCount, 0);
    const int first = itemsPage * rowsOnPage;
    const int onPage = count - first < rowsOnPage ? count - first : rowsOnPage;

    size_t n = 0;
    for (const char* c = list.name.c_str(); *c != '\0' && n + 1 < sizeof(shoutedTitle); ++c, ++n) {
      shoutedTitle[n] = *c >= 'a' && *c <= 'z' ? static_cast<char>(*c - 'a' + 'A') : *c;
    }
    shoutedTitle[n] = '\0';

    todoui::ItemRow rows[todoui::kMaxRowsOnPage];
    for (int i = 0; i < onPage; ++i) {
      rows[i].text = list.items[first + i].text.c_str();
      rows[i].complete = list.items[first + i].complete;
      rows[i].value = static_cast<int16_t>(first + i);
    }
    todoui::ItemsModel model;
    model.title = shoutedTitle;
    model.rows = rows;
    model.count = onPage > 0 ? onPage : 0;
    model.complete = todo::isComplete(list);
    model.empty = list.items.empty();
    model.page = itemsPage;
    model.pageCount = pageCount;
    todoui::buildItems(screen, model);
  }
  interactionsReady = true;

  toybox::reportOverflow(interactions, "To Do");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
