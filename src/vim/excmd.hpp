#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "doc/document.hpp"

namespace calc {

struct ExOutcome {
  std::string message;
  bool is_error = false;
  bool quit = false;
  std::optional<std::size_t> goto_row;
  std::optional<bool> line_numbers;
  std::optional<std::string> open_url;
};

ExOutcome execute_ex_command(std::string_view command, Document& document);

}
