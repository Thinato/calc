#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/engine.hpp"
#include "doc/document.hpp"

namespace calc {

// Per-line evaluation results, recomputed once per frame instead of inside the
// edit path.
//
// Invalidation compares each row against the text that was last evaluated for
// it. That is a string compare per line per frame — nothing for a scratchpad —
// and it removes any chance of a stale result caused by a missed invalidation
// signal, which is the usual failure mode of a dirty-flag scheme.
class ResultCache {
 public:
  void refresh(const Document& document);

  const LineEval& at(std::size_t row) const;
  std::size_t size() const { return entries_.size(); }

 private:
  struct Entry {
    std::string source;
    LineEval eval;
    bool evaluated = false;  // distinguishes "not yet run" from "ran, no result"
  };

  std::vector<Entry> entries_;
};

}  // namespace calc
