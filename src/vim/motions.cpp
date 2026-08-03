#include "vim/motions.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "doc/utf8.hpp"

namespace calc {
namespace {

enum class CharClass { Blank, Keyword, Punctuation };

CharClass classify(char byte) {
  if (byte == ' ' || byte == '\t' || byte == '\n') return CharClass::Blank;
  const auto value = static_cast<unsigned char>(byte);
  if (value >= 0x80) return CharClass::Keyword;
  if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
      (value >= '0' && value <= '9') || byte == '_') {
    return CharClass::Keyword;
  }
  return CharClass::Punctuation;
}

CharClass classify_big(char byte) {
  return classify(byte) == CharClass::Blank ? CharClass::Blank : CharClass::Keyword;
}

CharClass class_of(char byte, bool big) {
  return big ? classify_big(byte) : classify(byte);
}

char char_at(const Document& document, Cursor position) {
  if (position.column >= document.line_length(position.row)) return '\n';
  return document.line(position.row)[position.column];
}

bool at_empty_line(const Document& document, Cursor position) {
  return position.column == 0 && document.line_length(position.row) == 0;
}

bool advance(const Document& document, Cursor& position) {
  const std::size_t length = document.line_length(position.row);
  if (position.column < length) {
    position.column = utf8::next_boundary(document.line(position.row), position.column);
    return true;
  }
  if (position.row < document.last_row()) {
    position.row += 1;
    position.column = 0;
    return true;
  }
  return false;
}

bool retreat(const Document& document, Cursor& position) {
  if (position.column > 0) {
    position.column = utf8::prev_boundary(document.line(position.row), position.column);
    return true;
  }
  if (position.row > 0) {
    position.row -= 1;
    position.column = document.line_length(position.row);
    return true;
  }
  return false;
}

Cursor word_forward(const Document& document, Cursor position, bool big) {
  const CharClass start = class_of(char_at(document, position), big);
  if (start == CharClass::Blank) {
    if (!advance(document, position)) return position;
  } else {
    while (class_of(char_at(document, position), big) == start) {
      if (!advance(document, position)) return position;
    }
  }
  while (class_of(char_at(document, position), big) == CharClass::Blank) {
    if (at_empty_line(document, position)) break;
    if (!advance(document, position)) return position;
  }
  return position;
}

Cursor word_backward(const Document& document, Cursor position, bool big) {
  if (!retreat(document, position)) return position;
  while (class_of(char_at(document, position), big) == CharClass::Blank) {
    if (at_empty_line(document, position)) return position;
    if (!retreat(document, position)) return position;
  }
  const CharClass run = class_of(char_at(document, position), big);
  while (true) {
    Cursor probe = position;
    if (!retreat(document, probe)) break;
    if (class_of(char_at(document, probe), big) != run) break;
    position = probe;
  }
  return position;
}

Cursor word_end(const Document& document, Cursor position, bool big) {
  if (!advance(document, position)) return position;
  while (class_of(char_at(document, position), big) == CharClass::Blank) {
    if (!advance(document, position)) return position;
  }
  const CharClass run = class_of(char_at(document, position), big);
  while (true) {
    Cursor probe = position;
    if (!advance(document, probe)) break;
    if (class_of(char_at(document, probe), big) != run) break;
    position = probe;
  }
  return position;
}

std::optional<std::size_t> find_forward(std::string_view line, std::size_t from,
                                        std::string_view needle, int count) {
  std::size_t position = from;
  for (int step = 0; step < count; ++step) {
    const std::size_t found = line.find(needle, utf8::next_boundary(line, position));
    if (found == std::string_view::npos) return std::nullopt;
    position = found;
  }
  return position;
}

std::optional<std::size_t> find_backward(std::string_view line, std::size_t from,
                                         std::string_view needle, int count) {
  std::size_t position = from;
  for (int step = 0; step < count; ++step) {
    if (position == 0) return std::nullopt;
    const std::size_t found = line.rfind(needle, position - 1);
    if (found == std::string_view::npos) return std::nullopt;
    position = found;
  }
  return position;
}

char closing_for(char opening) {
  switch (opening) {
    case '(': return ')';
    case '[': return ']';
    case '{': return '}';
    default: return '\0';
  }
}

char opening_for(char closing) {
  switch (closing) {
    case ')': return '(';
    case ']': return '[';
    case '}': return '{';
    default: return '\0';
  }
}

std::optional<Cursor> matching_bracket(const Document& document, Cursor from) {
  const std::string& line = document.line(from.row);
  std::size_t column = from.column;
  while (column < line.size() && closing_for(line[column]) == '\0' &&
         opening_for(line[column]) == '\0') {
    column = utf8::next_boundary(line, column);
  }
  if (column >= line.size()) return std::nullopt;

  const char bracket = line[column];
  Cursor position{from.row, column};

  if (const char closing = closing_for(bracket); closing != '\0') {
    int depth = 0;
    while (true) {
      const char current = char_at(document, position);
      if (current == bracket) ++depth;
      if (current == closing && --depth == 0) return position;
      if (!advance(document, position)) return std::nullopt;
    }
  }

  const char opening = opening_for(bracket);
  int depth = 0;
  while (true) {
    const char current = char_at(document, position);
    if (current == bracket) ++depth;
    if (current == opening && --depth == 0) return position;
    if (!retreat(document, position)) return std::nullopt;
  }
}

bool is_blank_line(const Document& document, std::size_t row) {
  return document.line(row).find_first_not_of(" \t") == std::string::npos;
}

std::size_t paragraph_forward(const Document& document, std::size_t row, int count) {
  for (int step = 0; step < count; ++step) {
    std::size_t next = row + 1;
    while (next < document.line_count() && is_blank_line(document, next)) ++next;
    while (next < document.line_count() && !is_blank_line(document, next)) ++next;
    row = std::min(next, document.last_row());
  }
  return row;
}

std::size_t paragraph_backward(const Document& document, std::size_t row, int count) {
  for (int step = 0; step < count; ++step) {
    if (row == 0) break;
    std::size_t previous = row - 1;
    while (previous > 0 && is_blank_line(document, previous)) --previous;
    while (previous > 0 && !is_blank_line(document, previous)) --previous;
    row = previous;
  }
  return row;
}

}

std::size_t first_non_blank(const Document& document, std::size_t row) {
  const std::string& text = document.line(row);
  const std::size_t found = text.find_first_not_of(" \t");
  return found == std::string::npos ? 0 : found;
}

Cursor end_of_current_word(const Document& document, Cursor from, int count, bool big) {
  Cursor position = from;
  for (int step = 0; step < std::max(count, 1); ++step) {
    if (step > 0) {
      position = word_end(document, position, big);
      continue;
    }
    const CharClass run = class_of(char_at(document, position), big);
    while (true) {
      Cursor probe = position;
      if (!advance(document, probe)) break;
      if (class_of(char_at(document, probe), big) != run) break;
      position = probe;
    }
  }
  return position;
}

bool motion_needs_argument(char key) {
  return key == 'f' || key == 'F' || key == 't' || key == 'T';
}

bool is_motion(char key) {
  switch (key) {
    case 'h':
    case 'l':
    case 'j':
    case 'k':
    case '0':
    case '^':
    case '$':
    case '|':
    case 'w':
    case 'W':
    case 'b':
    case 'B':
    case 'e':
    case 'E':
    case 'G':
    case 'f':
    case 'F':
    case 't':
    case 'T':
    case '%':
    case '{':
    case '}': return true;
    default: return false;
  }
}

MotionResult apply_motion(const Document& document, Cursor from, char key, int count,
                          const std::string& argument, bool for_operator) {
  MotionResult result;
  result.target = from;
  const int repeat = std::max(count, 1);

  switch (key) {
    case 'h': {
      Cursor position = from;
      for (int step = 0; step < repeat && position.column > 0; ++step) {
        position.column =
            utf8::prev_boundary(document.line(position.row), position.column);
      }
      result.target = position;
      result.valid = true;
      break;
    }
    case 'l': {
      Cursor position = from;
      const std::size_t length = document.line_length(position.row);
      for (int step = 0; step < repeat && position.column < length; ++step) {
        position.column =
            utf8::next_boundary(document.line(position.row), position.column);
      }
      result.target = position;
      result.valid = true;
      break;
    }
    case 'j': {
      if (from.row == document.last_row()) break;
      result.target.row =
          std::min(from.row + static_cast<std::size_t>(repeat), document.last_row());
      result.kind = MotionKind::Linewise;
      result.valid = true;
      break;
    }
    case 'k': {
      if (from.row == 0) break;
      result.target.row = from.row >= static_cast<std::size_t>(repeat)
                              ? from.row - static_cast<std::size_t>(repeat)
                              : 0;
      result.kind = MotionKind::Linewise;
      result.valid = true;
      break;
    }
    case '0':
      result.target.column = 0;
      result.valid = true;
      break;
    case '^':
      result.target.column = first_non_blank(document, from.row);
      result.valid = true;
      break;
    case '$': {
      const std::size_t row =
          std::min(from.row + static_cast<std::size_t>(repeat) - 1, document.last_row());
      const std::size_t length = document.line_length(row);
      result.target.row = row;
      result.target.column = utf8::prev_boundary(document.line(row), length);
      result.kind = MotionKind::CharwiseInclusive;
      result.valid = true;
      break;
    }
    case '|': {
      const std::string& text = document.line(from.row);
      std::size_t column = 0;
      for (int step = 1; step < repeat && column < text.size(); ++step) {
        column = utf8::next_boundary(text, column);
      }
      result.target.column = column;
      result.valid = true;
      break;
    }
    case 'w':
    case 'W': {
      Cursor position = from;
      for (int step = 0; step < repeat; ++step) {
        position = word_forward(document, position, key == 'W');
      }
      if (for_operator && position.row != from.row) {
        position = Cursor{from.row, document.line_length(from.row)};
      }
      result.target = position;
      result.valid = true;
      break;
    }
    case 'b':
    case 'B': {
      Cursor position = from;
      for (int step = 0; step < repeat; ++step) {
        position = word_backward(document, position, key == 'B');
      }
      result.target = position;
      result.valid = true;
      break;
    }
    case 'e':
    case 'E': {
      Cursor position = from;
      for (int step = 0; step < repeat; ++step) {
        position = word_end(document, position, key == 'E');
      }
      result.target = position;
      result.kind = MotionKind::CharwiseInclusive;
      result.valid = true;
      break;
    }
    case 'G': {
      result.target.row =
          count > 0 ? std::min(static_cast<std::size_t>(count) - 1, document.last_row())
                    : document.last_row();
      result.target.column = first_non_blank(document, result.target.row);
      result.kind = MotionKind::Linewise;
      result.valid = true;
      break;
    }
    case 'f':
    case 'F':
    case 't':
    case 'T': {
      if (argument.empty()) break;
      const std::string& text = document.line(from.row);
      const bool forward = key == 'f' || key == 't';
      const std::optional<std::size_t> found =
          forward ? find_forward(text, from.column, argument, repeat)
                  : find_backward(text, from.column, argument, repeat);
      if (!found) break;
      std::size_t column = *found;
      if (key == 't') column = utf8::prev_boundary(text, column);
      if (key == 'T') column = utf8::next_boundary(text, column);
      result.target.column = column;
      result.kind =
          forward ? MotionKind::CharwiseInclusive : MotionKind::CharwiseExclusive;
      result.valid = true;
      break;
    }
    case '%': {
      const std::optional<Cursor> match = matching_bracket(document, from);
      if (!match) break;
      result.target = *match;
      result.kind = MotionKind::CharwiseInclusive;
      result.valid = true;
      break;
    }
    case '}': {
      result.target.row = paragraph_forward(document, from.row, repeat);
      result.target.column = 0;
      result.valid = true;
      break;
    }
    case '{': {
      result.target.row = paragraph_backward(document, from.row, repeat);
      result.target.column = 0;
      result.valid = true;
      break;
    }
    default: break;
  }
  return result;
}

}
