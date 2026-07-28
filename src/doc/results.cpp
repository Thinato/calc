#include "doc/results.hpp"

namespace calc {
namespace {

const LineEval& empty_eval() {
  static const LineEval kEmpty;
  return kEmpty;
}

}  // namespace

void ResultCache::refresh(const Document& document) {
  entries_.resize(document.line_count());
  for (std::size_t row = 0; row < document.line_count(); ++row) {
    Entry& entry = entries_[row];
    const std::string& source = document.line(row);
    if (entry.evaluated && entry.source == source) continue;
    entry.source = source;
    entry.eval = evaluate_line(source);
    entry.evaluated = true;
  }
}

const LineEval& ResultCache::at(std::size_t row) const {
  if (row >= entries_.size()) return empty_eval();
  return entries_[row].eval;
}

}  // namespace calc
