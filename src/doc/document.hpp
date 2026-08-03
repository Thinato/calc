#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace calc {

struct Cursor {
  std::size_t row = 0;
  std::size_t column = 0;
};

bool operator==(const Cursor& lhs, const Cursor& rhs);
bool operator!=(const Cursor& lhs, const Cursor& rhs);
bool operator<(const Cursor& lhs, const Cursor& rhs);

class Document {
 public:
  Document();
  static Document from_text(std::string_view text);

  std::string to_text() const;

  std::size_t line_count() const { return lines_.size(); }
  std::size_t last_row() const { return lines_.size() - 1; }
  const std::vector<std::string>& lines() const { return lines_; }
  const std::string& line(std::size_t row) const;
  std::size_t line_length(std::size_t row) const;
  bool empty() const;

  std::string text_range(Cursor from, Cursor to) const;
  std::string lines_text(std::size_t first, std::size_t count) const;

  Cursor cursor() const { return cursor_; }
  void set_cursor(Cursor cursor);
  Cursor clamped(Cursor cursor, bool allow_past_end) const;

  std::size_t revision() const { return revision_; }

  void insert_text(Cursor at, std::string_view text);
  Cursor insert_text_multiline(Cursor at, std::string_view text);
  std::string erase_range(Cursor from, Cursor to);
  std::string erase_lines(std::size_t first, std::size_t count);
  void insert_lines(std::size_t before_row, const std::vector<std::string>& text);
  void replace_line(std::size_t row, std::string text);
  void split_line(Cursor at);
  bool join_lines(std::size_t row);

  void begin_change();
  void commit_change();
  bool undo();
  bool redo();

  const std::string& path() const { return path_; }
  void set_path(std::string path) { path_ = std::move(path); }
  bool modified() const { return modified_; }
  void mark_saved() { modified_ = false; }

 private:
  struct Snapshot {
    std::vector<std::string> lines;
    Cursor cursor;
  };

  void touch();

  std::vector<std::string> lines_{std::string()};
  Cursor cursor_;
  std::string path_;
  bool modified_ = false;
  std::size_t revision_ = 0;

  std::vector<Snapshot> undo_stack_;
  std::vector<Snapshot> redo_stack_;
  bool change_open_ = false;
};

}
