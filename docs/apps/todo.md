# To Do

Lists of things to do, on the device. Several lists, each a column of items
with a box you tap to tick. Nothing here needs the network: a list is a few
hundred bytes in one text file on the card, written every time something
changes and read when the app opens.

It is on the shelf as **Apps > TO DO**, in `src/apps_local/todo/`.

## The main screen

Every list, one row each: a box at the left, the name, a line under it saying
how much is left (`3 OPEN`, `COMPLETE`, or `NO ITEMS`), and a `...` control
at the right. A pinned list carries a pin beside the `...`.

- Tap the row to open the list.
- Tap the `...` for the list's menu: **Pin to top** or **Unpin**, **Mark
  complete** or **Mark incomplete**, and **Delete**. The menu is titled with
  the list's name, so a menu opened on the wrong row says so before anything
  is done. Delete asks nothing further.
- **NEW LIST**, across the foot, opens the keyboard. An empty name creates
  nothing; so does cancelling.

The rows are in four groups, top to bottom: pinned lists you are still working
on, the rest you are still working on, pinned lists you have finished, the
rest you have finished. Within a group, lists keep the order they were made
in, whatever happens to the lists around them: ticking a neighbour's last item
sinks that neighbour and moves nothing else.

A list is **complete** when it has at least one item and every item is
ticked. A list with no items is not complete. That is derived from the items
every time it is asked; nothing stores it, so nothing can disagree with it.

## A list

The list's name in the band, its items below, and at the foot a square with an
**X** and the **ADD ITEM** pill.

- Tap an item anywhere on its row to tick it, and again to untick it. The box
  fills with a tick.
- When every item is ticked the line under the band says **COMPLETE**. Untick
  one and it goes.
- **ADD ITEM** opens the keyboard. Empty text adds nothing.
- Back returns to the lists.

Names and items wrap onto up to three lines, and a row is as tall as its text.
The keyboard stops at 48 characters, which is fewer than three lines hold at
the narrowest row, so anything typed on the device is shown whole. Only a file
edited by hand with longer lines is cut, with an ellipsis.

### Removing items: the bin

The X square at the left of the foot is the bin.

1. Tap it. The square turns inside out, the line under the band reads **TAP
   ITEMS TO REMOVE**, and ADD ITEM dims and stops answering.
2. Tap the items to remove. Each one's box shows an X in place of its tick or
   its blank; tap again to change your mind. The rows do not move.
3. Tap the bin again. The marked items go, the list is saved, and the mode
   closes. With nothing marked it only closes.

Back also closes bin mode, and forgets the marks: nothing is removed until
the bin is pressed the second time.

## Paging

A page holds as many rows as fit. When there is more than one page the band
shows `2/3` at its right, and the two side keys or a vertical swipe turn the
pages, stopping at both ends. A list or item you have just added is brought
into view.

## The file on the card

`/.crosspoint/todo.txt`, beside the reader's own state and every game's save,
so clearing `.crosspoint/` clears this too. One entry per line:

```
# CrossPlay TO DO v1
L|1|Shopping
I|0|Milk
I|1|Coffee
L|0|Work
```

`L|<pinned>|<name>` opens a list and `I|<complete>|<text>` adds an item to the
list most recently opened. The flag is `0` or `1`. The text is everything
after the second bar, so a bar inside a name needs no escaping. Lists are
written in the order they were made; the order on screen is derived from
them, not stored.

Reading is forgiving: a line that is not an entry, an item before any list, a
blank name or text, and anything past the limits below is skipped, and the
rest of the file is kept. A missing file is an app with no lists. The first
line names the version so a later format can still read this one.

The limits are 32 lists, 64 items a list, and 48 characters a name or item.

The app writes the whole file after every change that matters: a list made or
deleted, pinned or unpinned, marked complete or incomplete, an item added,
ticked, or removed. Paging and opening a list write nothing.

## What it does not do

No due dates, reminders, priorities, colours, categories, subtasks, recurring
items, reordering by hand, editing an item's text after it is made, or
syncing anywhere. Those are what a phone is for; this is the list you carry
on the device you already have in your hand.

## Where the code is

| Layer    | File                        | Knows about                                        |
| -------- | --------------------------- | -------------------------------------------------- |
| Core     | `TodoCore.{h,cpp}`          | lists, items, completion, the order, the file      |
| Screens  | `TodoScreens.{h,cpp}`       | the two screens, FreeInkUI and Toybox tokens only  |
| Activity | `TodoActivity.{h,cpp}`      | the renderer, the card, the keyboard, the menu     |

`host-tests/todo/run.sh` builds the first two without a device and checks the
completion rule, the order, the file round trip and what a damaged file costs,
that a wrapped row grows by exactly the lines it uses and moves the rows below
it, and that every control is registered against the rect it is drawn in.

The icons (the shelf's list, the pin, the `...` and the X) are Lucide's,
generated into `TodoIcons.h` by `tools_local/todo/gen_todo_icons.sh` from
`tools_local/todo/icons.txt`.
