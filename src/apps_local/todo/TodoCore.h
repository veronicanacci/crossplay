#pragma once

// TO DO: the lists, the one rule about when a list is done, the order they are
// shown in, and the file they live in.
//
// Freestanding: <string> and <vector> and nothing else. No renderer, no
// Arduino, no SDK and no SD card, so host-tests/todo can build the whole model
// and drive it without a device. The Activity owns the card; this owns what is
// written to it.

#include <string>
#include <vector>

namespace todo {

// ---- limits -----------------------------------------------------------------

// Enough for a person, small enough that the file is read whole into RAM on an
// ESP32: 32 lists of 64 items of 48 characters is under 100 KB at the very
// worst, and a real card holds a few hundred bytes of it.
constexpr int kMaxLists = 32;
constexpr int kMaxItems = 64;
// Characters, not bytes: this is what the keyboard is told, and the keyboard
// counts code points.
constexpr int kMaxTextChars = 48;

// Beside the reader's own state, the shelf's and every game's save, so clearing
// `.crosspoint/` clears this too. One text file for every list: a to-do list is
// a few hundred bytes and one write per change is the simplest thing that
// survives a reboot.
constexpr char kSavePath[] = "/.crosspoint/todo.txt";

// ---- the model --------------------------------------------------------------

struct Item {
  std::string text;
  bool complete = false;
};

// No `complete` field, on purpose. A list is complete when its items are, and a
// stored flag beside them is a second copy of that fact that can disagree with
// the first. See isComplete().
struct List {
  std::string name;
  bool pinned = false;
  std::vector<Item> items;
};

// Non-empty, and every item complete. An empty list has nothing done and
// nothing to do, and calling it complete would tick every list the moment it
// was created.
bool isComplete(const List& list);

// Items still open. What the main screen says under a list's name.
int openCount(const List& list);

// What a typed string becomes before it is kept: whitespace trimmed from both
// ends, line breaks and tabs inside it flattened to spaces (the file is one
// entry per line, and a name with a newline in it would be two entries). An
// all-whitespace string comes back empty, which is how "do not create blank
// items" is decided in one place.
std::string cleaned(const std::string& text);

// ---- the order --------------------------------------------------------------

// The four groups the main screen shows, top to bottom. Pinned lists you are
// still working on, then the rest you are working on, then the pinned ones you
// have finished, then the rest. Finished lists sink; pinned ones float within
// their half.
//   0  pinned, incomplete
//   1  unpinned, incomplete
//   2  pinned, complete
//   3  unpinned, complete
int sortGroup(const List& list);

// Indexes into `lists`, in display order. A STABLE sort by group, so two lists
// in the same group keep the order they were created in whatever happens to
// the lists around them -- a list does not jump when a neighbour is ticked.
std::vector<int> displayOrder(const std::vector<List>& lists);

// ---- the edits --------------------------------------------------------------

// Every change the app can make, each one validated here rather than at the
// button that asks for it. Each returns whether anything changed; the caller
// saves when one does and not otherwise.
struct Store {
  std::vector<List> lists;

  // The new list's index, or -1: an empty name (after cleaning) or a full
  // store adds nothing.
  int addList(const std::string& name);
  bool removeList(int index);
  bool setPinned(int index, bool pinned);
  // Every item in the list to `complete`. "Mark complete" and "Mark incomplete"
  // on the main screen, and the only way a list's completion is set directly
  // -- by setting the items, so isComplete() stays derived.
  bool markAll(int index, bool complete);
  // The new item's index, or -1, on the same terms as addList.
  int addItem(int list, const std::string& text);
  bool toggleItem(int list, int item);

  bool validList(int index) const { return index >= 0 && index < static_cast<int>(lists.size()); }
};

// ---- paging -----------------------------------------------------------------

// How many pages `count` rows need at `perPage`, never fewer than one.
int pageCountFor(int count, int perPage);
// The page holding row `index`.
int pageFor(int index, int perPage);
// `page` moved by `delta`, stopping at both ends. Stopping rather than wrapping:
// every page draws its rows in the same places, so a wrap arrived at by
// accident looks exactly like the page that was wanted, and the next tap opens
// the wrong list.
int pageStep(int page, int pageCount, int delta);

// ---- the file ---------------------------------------------------------------

// One entry per line, in the order lists are kept (creation order; the display
// order is derived, not stored):
//
//     # CrossPlay TO DO v1
//     L|1|Shopping
//     I|0|Milk
//     I|1|Coffee
//     L|0|Work
//
// `L|<pinned>|<name>` opens a list, `I|<complete>|<text>` adds an item to the
// list most recently opened. The text is everything after the second bar, so
// a bar in a name needs no escaping: the two fields before it are fixed width.
// A text file rather than bytes because the owner of the card may want to read
// it, and because a version number on the first line is what lets v2 read v1.
std::string encode(const std::vector<List>& lists);

// Reads what it can and skips what it cannot: a line that is not an entry, an
// item before any list, an entry past the limits, an empty name. Never fails
// -- a corrupt file costs the lines that are corrupt, not the app. `out` is
// replaced.
void decode(const std::string& text, std::vector<List>& out);

}  // namespace todo
