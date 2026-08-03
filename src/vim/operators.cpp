#include "vim/operators.hpp"

#include <algorithm>
#include <utility>

#include "doc/utf8.hpp"

namespace calc {

Range resolve_range(const Document& document, Cursor start, const MotionResult& motion) {
  Range range;

  if (motion.kind == MotionKind::Linewise) {
    const std::size_t first = std::min(start.row, motion.target.row);
    const std::size_t last = std::max(start.row, motion.target.row);
    range.linewise = true;
    range.first_row = first;
    range.row_count = last - first + 1;
    return range;
  }

  Cursor from = start;
  Cursor to = motion.target;
  if (to < from) std::swap(from, to);

  if (motion.kind == MotionKind::CharwiseInclusive) {
    to.column = utf8::next_boundary(document.line(to.row), to.column);
  }

  range.from = from;
  range.to = to;
  return range;
}

}
