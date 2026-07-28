#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "doc/document.hpp"

namespace calc {

// What a ':' command asks the editor to do. Returning intentions instead of
// acting on engine state keeps this layer independent and directly testable.
struct ExOutcome {
  std::string message;
  bool is_error = false;
  bool quit = false;
  std::optional<std::size_t> goto_row;      // 0-based
  std::optional<bool> line_numbers;         // :set number / :set nonumber
};

// `command` is the text after the ':'. Writing and reading files happens here,
// via Document::to_text(), which is why a save can never emit a result column.
ExOutcome execute_ex_command(std::string_view command, Document& document);

}  // namespace calc
