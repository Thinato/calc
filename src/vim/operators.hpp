#pragma once

#include <cstddef>
#include <string>

#include "doc/document.hpp"
#include "vim/motions.hpp"

namespace calc {

enum class Operator { None, Delete, Change, Yank };

// A register holds text plus how it was captured, because `p` puts a linewise
// yank on a new line and a charwise yank inline.
struct Register {
  std::string text;
  bool linewise = false;
};

// A normalized span of the buffer for an operator to act on.
struct Range {
  bool linewise = false;
  Cursor from;  // charwise: inclusive start
  Cursor to;    // charwise: exclusive end
  std::size_t first_row = 0;
  std::size_t row_count = 0;
};

// Turns a start position plus a motion into the span the operator consumes.
// This is where CharwiseInclusive becomes an exclusive end, so every caller
// downstream deals with one representation.
Range resolve_range(const Document& document, Cursor start, const MotionResult& motion);

}  // namespace calc
