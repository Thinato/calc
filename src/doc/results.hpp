#pragma once

#include <cstddef>
#include <vector>

#include "core/engine.hpp"
#include "core/environment.hpp"
#include "doc/document.hpp"

namespace calc {

// Per-line evaluation results, recomputed once per frame instead of inside the
// edit path.
//
// Names flow top to bottom, so a change anywhere can alter every line below it —
// a line whose own text did not change may still need a new result. The whole
// file is therefore re-evaluated in one pass, guarded on Document::revision(),
// which moves on every mutation and is an exact signal. That is simpler than a
// dependency graph and cannot go stale, and a scratchpad is far too small for the
// difference in cost to show.
class ResultCache {
 public:
  void refresh(const Document& document);

  const LineEval& at(std::size_t row) const;
  std::size_t size() const { return entries_.size(); }

  // The names in effect at the end of the file.
  const Environment& environment() const { return environment_; }

 private:
  std::vector<LineEval> entries_;
  Environment environment_;
  std::size_t revision_ = 0;
  bool primed_ = false;
};

}  // namespace calc
