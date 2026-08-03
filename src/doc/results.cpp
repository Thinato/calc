#include "doc/results.hpp"

namespace calc {
namespace {

const LineEval& empty_eval() {
  static const LineEval kEmpty;
  return kEmpty;
}

}

void ResultCache::refresh(const Document& document) {
  if (primed_ && revision_ == document.revision()) return;
  revision_ = document.revision();
  primed_ = true;

  environment_.reset();
  entries_.assign(document.line_count(), LineEval{});
  for (std::size_t row = 0; row < document.line_count(); ++row) {
    entries_[row] = evaluate_line(document.line(row), environment_, row);
  }
}

const LineEval& ResultCache::at(std::size_t row) const {
  if (row >= entries_.size()) return empty_eval();
  return entries_[row];
}

}
