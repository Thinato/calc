#pragma once

#include <cstddef>
#include <string_view>

// Cursor columns are byte offsets. These helpers keep every column the program
// produces on a UTF-8 character boundary, so a multi-byte character in a
// comment cannot be sliced in half by h/l/x.
namespace calc::utf8 {

bool is_continuation(char byte);

// Offset of the character after the one starting at `index`. Clamped to size().
std::size_t next_boundary(std::string_view text, std::size_t index);

// Offset of the character before `index`. Clamped to 0.
std::size_t prev_boundary(std::string_view text, std::size_t index);

// Number of characters (not bytes) in the text.
std::size_t count_chars(std::string_view text);

// Number of characters before the byte offset `index`; the display column.
std::size_t chars_before(std::string_view text, std::size_t index);

}  // namespace calc::utf8
