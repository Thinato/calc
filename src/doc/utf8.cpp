#include "doc/utf8.hpp"

namespace calc::utf8 {

bool is_continuation(char byte) {
  return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
}

std::size_t next_boundary(std::string_view text, std::size_t index) {
  if (index >= text.size()) return text.size();
  std::size_t next = index + 1;
  while (next < text.size() && is_continuation(text[next])) ++next;
  return next;
}

std::size_t prev_boundary(std::string_view text, std::size_t index) {
  if (index == 0) return 0;
  std::size_t previous = index - 1;
  while (previous > 0 && is_continuation(text[previous])) --previous;
  return previous;
}

std::size_t count_chars(std::string_view text) {
  std::size_t count = 0;
  for (char byte : text) {
    if (!is_continuation(byte)) ++count;
  }
  return count;
}

std::size_t chars_before(std::string_view text, std::size_t index) {
  if (index > text.size()) index = text.size();
  return count_chars(text.substr(0, index));
}

}  // namespace calc::utf8
