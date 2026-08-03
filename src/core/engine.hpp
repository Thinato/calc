#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/blocks.hpp"
#include "core/environment.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

struct LineEval {
  std::optional<Value> value;
  std::string text;
  std::optional<Error> error;

  std::optional<std::string> assigned_name;
  std::size_t assigned_column = 0;
  bool assigned_constant = false;

  std::optional<std::string> defined_name;
  std::size_t defined_column = 0;

  bool show_result = true;

  bool has_result() const { return value.has_value(); }
  bool is_assignment() const { return assigned_name.has_value(); }
  bool is_definition() const { return defined_name.has_value(); }
};

LineEval evaluate_line(std::string_view line, Environment& environment, std::size_t row);

LineEval evaluate_line(std::string_view line);

struct UnitEval {
  LineEval eval;
  std::size_t error_row = 0;
};

UnitEval evaluate_unit(const std::vector<std::string>& lines, Unit unit,
                       Environment& environment);

}
