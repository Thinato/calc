#include "doc/document.hpp"

#include <algorithm>
#include <utility>

#include "doc/utf8.hpp"

namespace calc {

bool operator==(const Cursor& lhs, const Cursor& rhs) {
  return lhs.row == rhs.row && lhs.column == rhs.column;
}
bool operator!=(const Cursor& lhs, const Cursor& rhs) { return !(lhs == rhs); }
bool operator<(const Cursor& lhs, const Cursor& rhs) {
  if (lhs.row != rhs.row) return lhs.row < rhs.row;
  return lhs.column < rhs.column;
}

namespace {

constexpr std::size_t kMaxUndoDepth = 500;

}

Document::Document() = default;

Document Document::from_text(std::string_view text) {
  Document document;
  document.lines_.clear();

  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t newline = text.find('\n', start);
    if (newline == std::string_view::npos) {
      document.lines_.emplace_back(text.substr(start));
      break;
    }
    document.lines_.emplace_back(text.substr(start, newline - start));
    start = newline + 1;
  }

  if (document.lines_.size() > 1 && document.lines_.back().empty()) {
    document.lines_.pop_back();
  }
  if (document.lines_.empty()) document.lines_.emplace_back();
  return document;
}

std::string Document::to_text() const {
  std::string text;
  for (const std::string& line : lines_) {
    text += line;
    text += '\n';
  }
  return text;
}

const std::string& Document::line(std::size_t row) const {
  return lines_[std::min(row, last_row())];
}

std::size_t Document::line_length(std::size_t row) const { return line(row).size(); }

bool Document::empty() const { return lines_.size() == 1 && lines_[0].empty(); }

Cursor Document::clamped(Cursor cursor, bool allow_past_end) const {
  cursor.row = std::min(cursor.row, last_row());
  const std::size_t length = line_length(cursor.row);
  const std::size_t limit = allow_past_end || length == 0
                                ? length
                                : utf8::prev_boundary(line(cursor.row), length);
  cursor.column = std::min(cursor.column, limit);
  const std::string& text = line(cursor.row);
  while (cursor.column > 0 && cursor.column < text.size() &&
         utf8::is_continuation(text[cursor.column])) {
    --cursor.column;
  }
  return cursor;
}

void Document::set_cursor(Cursor cursor) { cursor_ = clamped(cursor, true); }

std::string Document::text_range(Cursor from, Cursor to) const {
  if (to < from) std::swap(from, to);
  if (from.row == to.row) {
    const std::string& text = line(from.row);
    const std::size_t begin = std::min(from.column, text.size());
    const std::size_t end = std::min(to.column, text.size());
    return text.substr(begin, end - begin);
  }

  std::string result =
      line(from.row).substr(std::min(from.column, line_length(from.row)));
  result += '\n';
  for (std::size_t row = from.row + 1; row < to.row; ++row) {
    result += line(row);
    result += '\n';
  }
  result += line(to.row).substr(0, std::min(to.column, line_length(to.row)));
  return result;
}

std::string Document::lines_text(std::size_t first, std::size_t count) const {
  std::string result;
  const std::size_t end = std::min(first + count, lines_.size());
  for (std::size_t row = first; row < end; ++row) {
    result += lines_[row];
    result += '\n';
  }
  return result;
}

void Document::touch() {
  begin_change();
  modified_ = true;
  ++revision_;
}

void Document::insert_text(Cursor at, std::string_view text) {
  touch();
  at = clamped(at, true);
  lines_[at.row].insert(at.column, text);
}

Cursor Document::insert_text_multiline(Cursor at, std::string_view text) {
  touch();
  at = clamped(at, true);

  const std::size_t first_newline = text.find('\n');
  if (first_newline == std::string_view::npos) {
    lines_[at.row].insert(at.column, text);
    return Cursor{at.row, at.column + text.size()};
  }

  std::string tail = lines_[at.row].substr(at.column);
  lines_[at.row].erase(at.column);
  lines_[at.row].append(text.substr(0, first_newline));

  std::size_t row = at.row;
  std::size_t start = first_newline + 1;
  while (true) {
    const std::size_t newline = text.find('\n', start);
    const std::string_view segment = newline == std::string_view::npos
                                         ? text.substr(start)
                                         : text.substr(start, newline - start);
    ++row;
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(row),
                  std::string(segment));
    if (newline == std::string_view::npos) break;
    start = newline + 1;
  }

  const Cursor end{row, lines_[row].size()};
  lines_[row] += tail;
  return end;
}

std::string Document::erase_range(Cursor from, Cursor to) {
  if (to < from) std::swap(from, to);
  const std::string removed = text_range(from, to);
  if (removed.empty()) return removed;
  touch();

  from = clamped(from, true);
  to = clamped(to, true);

  if (from.row == to.row) {
    lines_[from.row].erase(from.column, to.column - from.column);
    return removed;
  }

  lines_[from.row] =
      lines_[from.row].substr(0, from.column) + lines_[to.row].substr(to.column);
  lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(from.row) + 1,
               lines_.begin() + static_cast<std::ptrdiff_t>(to.row) + 1);
  return removed;
}

std::string Document::erase_lines(std::size_t first, std::size_t count) {
  if (first > last_row() || count == 0) return {};
  count = std::min(count, lines_.size() - first);
  const std::string removed = lines_text(first, count);
  touch();

  lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(first),
               lines_.begin() + static_cast<std::ptrdiff_t>(first + count));
  if (lines_.empty()) lines_.emplace_back();
  return removed;
}

void Document::insert_lines(std::size_t before_row,
                            const std::vector<std::string>& text) {
  if (text.empty()) return;
  touch();
  before_row = std::min(before_row, lines_.size());
  lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(before_row), text.begin(),
                text.end());
}

void Document::replace_line(std::size_t row, std::string text) {
  if (row > last_row()) return;
  touch();
  lines_[row] = std::move(text);
}

void Document::split_line(Cursor at) {
  touch();
  at = clamped(at, true);
  std::string tail = lines_[at.row].substr(at.column);
  lines_[at.row].erase(at.column);
  lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(at.row) + 1,
                std::move(tail));
}

bool Document::join_lines(std::size_t row) {
  if (row >= last_row()) return false;
  touch();
  lines_[row] += lines_[row + 1];
  lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(row) + 1);
  return true;
}

void Document::begin_change() {
  if (change_open_) return;
  change_open_ = true;
  undo_stack_.push_back(Snapshot{lines_, cursor_});
  if (undo_stack_.size() > kMaxUndoDepth) {
    undo_stack_.erase(undo_stack_.begin());
  }
  redo_stack_.clear();
}

void Document::commit_change() { change_open_ = false; }

bool Document::undo() {
  commit_change();
  if (undo_stack_.empty()) return false;
  redo_stack_.push_back(Snapshot{lines_, cursor_});
  Snapshot previous = std::move(undo_stack_.back());
  undo_stack_.pop_back();
  lines_ = std::move(previous.lines);
  cursor_ = clamped(previous.cursor, true);
  modified_ = true;
  ++revision_;
  return true;
}

bool Document::redo() {
  commit_change();
  if (redo_stack_.empty()) return false;
  undo_stack_.push_back(Snapshot{lines_, cursor_});
  Snapshot next = std::move(redo_stack_.back());
  redo_stack_.pop_back();
  lines_ = std::move(next.lines);
  cursor_ = clamped(next.cursor, true);
  modified_ = true;
  ++revision_;
  return true;
}

}
