#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/plot.hpp"
#include "doc/document.hpp"

namespace calc {

class ResultCache;

struct ExOutcome {
  std::string message;
  bool is_error = false;
  bool quit = false;
  std::optional<std::size_t> goto_row;
  std::optional<bool> line_numbers;
  std::optional<std::string> open_url;
  std::optional<PlotSpec> plot;
  bool close_plot = false;
  std::optional<InfinityMode> infinity_mode;
};

ExOutcome execute_ex_command(std::string_view command, Document& document,
                             const ResultCache* results = nullptr);

}
