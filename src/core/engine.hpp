#pragma once

#include <optional>
#include <string>
#include <string_view>

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

  bool has_result() const { return value.has_value(); }
};

LineEval evaluate_line(std::string_view line);

}  // namespace calc
