#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace calc {

// A position in the buffer. `column` is a byte offset into the line's text.
struct Cursor {
  std::size_t row = 0;
  std::size_t column = 0;
};

bool operator==(const Cursor& lhs, const Cursor& rhs);
bool operator!=(const Cursor& lhs, const Cursor& rhs);
bool operator<(const Cursor& lhs, const Cursor& rhs);

// The editable buffer.
//
// A line holds *only* what the user typed. Computed results live outside the
// document (see ResultCache) and are drawn by the renderer. That is what makes
// the "= 3" suffix impossible to edit: there is no text there to address, so no
// motion, operator or paste can reach it. set_cursor() additionally clamps
// every column into [0, line_length], which is the one place that invariant is
// enforced.
class Document {
 public:
  Document();
  static Document from_text(std::string_view text);

  // Newline-joined lines with a trailing newline. Results never appear here,
  // which is why saving a file cannot write them.
  std::string to_text() const;

  // ------------------------------------------------------------------ reading
  std::size_t line_count() const { return lines_.size(); }
  std::size_t last_row() const { return lines_.size() - 1; }
  const std::vector<std::string>& lines() const { return lines_; }
  const std::string& line(std::size_t row) const;
  std::size_t line_length(std::size_t row) const;
  bool empty() const;  // a single empty line

  std::string text_range(Cursor from, Cursor to) const;
  std::string lines_text(std::size_t first, std::size_t count) const;

  // ------------------------------------------------------------------- cursor
  Cursor cursor() const { return cursor_; }
  void set_cursor(Cursor cursor);
  // Clamps a position into the document. `allow_past_end` permits the column to
  // equal the line length, which insert mode needs and normal mode does not.
  Cursor clamped(Cursor cursor, bool allow_past_end) const;

  // Bumped by every mutation. The engine compares it before and after a command
  // to tell whether that command changed anything, which is how `.` knows what
  // is worth repeating.
  std::size_t revision() const { return revision_; }

  // -------------------------------------------------------------------- edits
  void insert_text(Cursor at, std::string_view text);
  // Inserts text that may contain newlines, splitting lines as needed. Returns
  // the position just past the inserted text.
  Cursor insert_text_multiline(Cursor at, std::string_view text);
  std::string erase_range(Cursor from, Cursor to);  // charwise, [from, to)
  std::string erase_lines(std::size_t first, std::size_t count);
  void insert_lines(std::size_t before_row, const std::vector<std::string>& text);
  void replace_line(std::size_t row, std::string text);
  void split_line(Cursor at);
  bool join_lines(std::size_t row);  // appends row + 1 onto row

  // --------------------------------------------------------------------- undo
  // Mutations between begin_change() and commit_change() collapse into one undo
  // step, so a whole insert session reverts as a unit.
  void begin_change();
  void commit_change();
  bool undo();
  bool redo();

  // --------------------------------------------------------------------- file
  const std::string& path() const { return path_; }
  void set_path(std::string path) { path_ = std::move(path); }
  bool modified() const { return modified_; }
  void mark_saved() { modified_ = false; }

 private:
  struct Snapshot {
    std::vector<std::string> lines;
    Cursor cursor;
  };

  void touch();  // marks modified; opens an undo step if none is open

  std::vector<std::string> lines_{std::string()};
  Cursor cursor_;
  std::string path_;
  bool modified_ = false;
  std::size_t revision_ = 0;

  std::vector<Snapshot> undo_stack_;
  std::vector<Snapshot> redo_stack_;
  bool change_open_ = false;
};

}  // namespace calc
