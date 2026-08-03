#pragma once

#include <cstddef>
#include <string_view>

namespace calc::utf8 {

bool is_continuation(char byte);

std::size_t next_boundary(std::string_view text, std::size_t index);

std::size_t prev_boundary(std::string_view text, std::size_t index);

std::size_t count_chars(std::string_view text);

std::size_t chars_before(std::string_view text, std::size_t index);

}
