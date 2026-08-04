#include "doc/results.hpp"

#include <utility>

#include "core/blocks.hpp"

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
  function_rows_.clear();
  entries_.assign(document.line_count(), LineEval{});
  definition_rows_.assign(document.line_count(), std::nullopt);

  for (const Unit& unit : scan_units(document.lines())) {
    UnitEval outcome = evaluate_unit(document.lines(), unit, environment_);
    if (outcome.eval.defined_name.has_value()) {
      function_rows_.try_emplace(*outcome.eval.defined_name, unit.first_row);
      for (std::size_t offset = 0; offset < unit.row_count; ++offset) {
        definition_rows_[unit.first_row + offset] = *outcome.eval.defined_name;
      }
    }
    entries_[outcome.error_row] = std::move(outcome.eval);
  }
}

const LineEval& ResultCache::at(std::size_t row) const {
  if (row >= entries_.size()) return empty_eval();
  return entries_[row];
}

bool ResultCache::is_function_at(std::string_view name, std::size_t row) const {
  const auto found = function_rows_.find(name);
  return found != function_rows_.end() && found->second <= row;
}

std::optional<std::string> ResultCache::definition_at(std::size_t row) const {
  if (row >= definition_rows_.size()) return std::nullopt;
  return definition_rows_[row];
}

}
