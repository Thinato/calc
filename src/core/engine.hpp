#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/environment.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

// The outcome of evaluating one line of the buffer.
//
// A blank line or a comment-only line produces neither a result nor an error:
// there was simply nothing to compute. A half-typed expression produces an
// error but no result — the renderer shows nothing inline and the status bar
// explains it only for the line the cursor is on.
struct LineEval {
  std::optional<Value> value;
  std::string text;  // formatted `value`, empty when there is none
  std::optional<Error> error;

  // Set when the line defined a name, for highlighting the target.
  std::optional<std::string> assigned_name;
  std::size_t assigned_column = 0;
  bool assigned_constant = false;

  // False when a definition's value was typed out literally, so the renderer can
  // leave `x = 128.40` alone instead of restating it as `= 128.4`.
  bool show_result = true;

  bool has_result() const { return value.has_value(); }
  bool is_assignment() const { return assigned_name.has_value(); }
};

// Evaluates one line against `environment`, defining a name when the line is an
// assignment. `row` is recorded on the binding so a later attempt to reassign a
// constant can say which line first defined it.
LineEval evaluate_line(std::string_view line, Environment& environment, std::size_t row);

// Evaluates a line on its own, against a fresh environment holding just the
// built-in constants.
LineEval evaluate_line(std::string_view line);

}  // namespace calc
