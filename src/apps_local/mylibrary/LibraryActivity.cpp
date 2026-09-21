#include "LibraryActivity.h"

#include <Memory.h>

#include <cstdio>
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
constexpr size_t kTagChars = 120;
constexpr size_t kNoteChars = 200;

// The home screen's two rows.
constexpr const char* kHomeRows[] = {"MY BOOKS", "WISHLIST"};

}  // namespace

std::unique_ptr<Activity> LibraryActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  // Never a bare new: the firmware is built -fno-exceptions, so a failed
  // allocation aborts rather than throwing.
  return makeUniqueNoThrow<LibraryActivity>(renderer, mappedInput);
}

void LibraryActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  books = library::sampleBooks();
  stack.clear();
  stack.push_back(View{});
  requestUpdate();
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
        if (row < 0 || row > 1) return;
        View next;
        next.kind = Kind::Collection;
        next.filter.collection = row == 0 ? library::Collection::MyBooks : library::Collection::Wishlist;
        push(next);
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
        options.push_back(book.read ? "Mark as unread" : "Mark as read");
        options.push_back(std::string("Rating: ") + library::ratingText(book.rating));
      }
      options.push_back(book.favourite ? "Remove from favourites" : "Add to favourites");
      options.push_back("Tags");
      options.push_back("Notes");
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
                  if (label.rfind("Mark as", 0) == 0) {
                    b.read = !b.read;
                  } else if (label.rfind("Rating", 0) == 0) {
                    pending = Pending::RatingMenu;
                  } else if (label.find("favourites") != std::string::npos) {
                    b.favourite = !b.favourite;
                  } else if (label == "Tags") {
                    pending = Pending::EditTags;
                  } else if (label == "Notes") {
                    pending = Pending::EditNotes;
                  }
                  requestUpdate();
                });
      break;
    }

    case Pending::EditTags:
      openKeyboard("Tags (semicolon separated)", book.tags, kTagChars,
                   [this, index](const std::string& text) { books[index].tags = text; });
      return;
    case Pending::EditNotes:
      openKeyboard("Notes", book.notes, kNoteChars,
                   [this, index](const std::string& text) { books[index].notes = text; });
      return;

    case Pending::RatingMenu: {
      const char* options[6] = {library::ratingText(0), library::ratingText(1), library::ratingText(2),
                                library::ratingText(3), library::ratingText(4), library::ratingText(5)};
      menu.show("Rating", options, 6, library::clampRating(book.rating), [this, index](const int choice) {
        books[index].rating = library::clampRating(choice);
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
    model.empty = libraryui::kNoGroups;
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
  if (library::isGrouped(view.filter.browse)) {
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
  model.plot = book.plot.c_str();
  model.showRead = book.collection == library::Collection::MyBooks;
  model.read = book.read;
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
  const bool jersey = view.kind == Kind::Home || view.kind == Kind::Collection || view.kind == Kind::Search;
  fui::GfxRendererTarget target =
      toybox::makeTarget(renderer, jersey ? toybox::toyboxFaces() : toybox::readingAddressFaces());
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};

  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);

  switch (view.kind) {
    case Kind::Home:
    case Kind::Collection:
    case Kind::Groups:
      renderMenu(screen, view);
      break;
    case Kind::Books:
      renderBooks(screen, view);
      break;
    case Kind::Detail:
      renderDetail(screen, view);
      break;
    case Kind::Search:
      libraryui::buildNotice(screen, "SEARCH", libraryui::kSearchSoon);
      break;
  }
  interactionsReady = true;

  toybox::reportOverflow(interactions, "My Library");

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
