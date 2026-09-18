#include "TodoCore.h"

#include <algorithm>

namespace todo {

bool isComplete(const List& list) {
  if (list.items.empty()) return false;
  for (const Item& item : list.items) {
    if (!item.complete) return false;
  }
  return true;
}

int openCount(const List& list) {
  int open = 0;
  for (const Item& item : list.items) {
    if (!item.complete) ++open;
  }
  return open;
}

namespace {

bool isBlank(const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

std::string cleaned(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    out.push_back(c == '\t' || c == '\r' || c == '\n' ? ' ' : c);
  }
  size_t start = 0;
  while (start < out.size() && isBlank(out[start])) ++start;
  size_t end = out.size();
  while (end > start && isBlank(out[end - 1])) --end;
  return out.substr(start, end - start);
}

int sortGroup(const List& list) {
  const int done = isComplete(list) ? 2 : 0;
  const int loose = list.pinned ? 0 : 1;
  return done + loose;
}

std::vector<int> displayOrder(const std::vector<List>& lists) {
  std::vector<int> order(lists.size());
  for (size_t i = 0; i < lists.size(); ++i) order[i] = static_cast<int>(i);
  std::stable_sort(order.begin(), order.end(),
                   [&lists](const int a, const int b) { return sortGroup(lists[a]) < sortGroup(lists[b]); });
  return order;
}

int Store::addList(const std::string& name) {
  const std::string clean = cleaned(name);
  if (clean.empty()) return -1;
  if (static_cast<int>(lists.size()) >= kMaxLists) return -1;
  List list;
  list.name = clean;
  lists.push_back(list);
  return static_cast<int>(lists.size()) - 1;
}

bool Store::removeList(const int index) {
  if (!validList(index)) return false;
  lists.erase(lists.begin() + index);
  return true;
}

bool Store::setPinned(const int index, const bool pinned) {
  if (!validList(index)) return false;
  if (lists[index].pinned == pinned) return false;
  lists[index].pinned = pinned;
  return true;
}

bool Store::markAll(const int index, const bool complete) {
  if (!validList(index)) return false;
  bool changed = false;
  for (Item& item : lists[index].items) {
    if (item.complete != complete) {
      item.complete = complete;
      changed = true;
    }
  }
  return changed;
}

int Store::addItem(const int list, const std::string& text) {
  if (!validList(list)) return -1;
  const std::string clean = cleaned(text);
  if (clean.empty()) return -1;
  std::vector<Item>& items = lists[list].items;
  if (static_cast<int>(items.size()) >= kMaxItems) return -1;
  Item item;
  item.text = clean;
  items.push_back(item);
  return static_cast<int>(items.size()) - 1;
}

bool Store::toggleItem(const int list, const int item) {
  if (!validList(list)) return false;
  std::vector<Item>& items = lists[list].items;
  if (item < 0 || item >= static_cast<int>(items.size())) return false;
  items[item].complete = !items[item].complete;
  return true;
}

int Store::removeItems(const int list, const std::vector<bool>& doomed) {
  if (!validList(list)) return 0;
  std::vector<Item>& items = lists[list].items;
  std::vector<Item> kept;
  kept.reserve(items.size());
  int removed = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    if (i < doomed.size() && doomed[i]) {
      ++removed;
    } else {
      kept.push_back(items[i]);
    }
  }
  if (removed > 0) items.swap(kept);
  return removed;
}

int pageStep(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  const int from = page < 0 ? 0 : (page >= pageCount ? pageCount - 1 : page);
  const int to = from + delta;
  if (to < 0) return 0;
  return to >= pageCount ? pageCount - 1 : to;
}

namespace {

constexpr char kHeader[] = "# CrossPlay TO DO v1";

void appendLine(std::string& out, const char kind, const bool flag, const std::string& text) {
  out.push_back(kind);
  out.push_back('|');
  out.push_back(flag ? '1' : '0');
  out.push_back('|');
  out += text;
  out.push_back('\n');
}

}  // namespace

std::string encode(const std::vector<List>& lists) {
  std::string out = kHeader;
  out.push_back('\n');
  for (const List& list : lists) {
    appendLine(out, 'L', list.pinned, list.name);
    for (const Item& item : list.items) appendLine(out, 'I', item.complete, item.text);
  }
  return out;
}

void decode(const std::string& text, std::vector<List>& out) {
  out.clear();
  // Set once the store is full, so the items of a list that was not kept are
  // not attached to the last list that was.
  bool dropping = false;
  size_t start = 0;
  while (start <= text.size()) {
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    std::string line = text.substr(start, end - start);
    start = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // An entry is at least `X|0|` plus one character of text.
    if (line.size() < 5 || line[1] != '|' || line[3] != '|') continue;
    if (line[2] != '0' && line[2] != '1') continue;
    const bool flag = line[2] == '1';
    const std::string body = cleaned(line.substr(4));
    if (body.empty()) continue;
    if (line[0] == 'L') {
      if (static_cast<int>(out.size()) >= kMaxLists) {
        dropping = true;
        continue;
      }
      dropping = false;
      List list;
      list.name = body;
      list.pinned = flag;
      out.push_back(list);
    } else if (line[0] == 'I') {
      if (out.empty() || dropping) continue;
      std::vector<Item>& items = out.back().items;
      if (static_cast<int>(items.size()) >= kMaxItems) continue;
      Item item;
      item.text = body;
      item.complete = flag;
      items.push_back(item);
    }
  }
}

}  // namespace todo
