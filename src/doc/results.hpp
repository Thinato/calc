#pragma once

#include <cstddef>
#include <vector>

#include "core/engine.hpp"
#include "core/environment.hpp"
#include "doc/document.hpp"

namespace calc {

class ResultCache {
 public:
  void refresh(const Document& document);

  const LineEval& at(std::size_t row) const;
  std::size_t size() const { return entries_.size(); }

  const Environment& environment() const { return environment_; }

 private:
  std::vector<LineEval> entries_;
  Environment environment_;
  std::size_t revision_ = 0;
  bool primed_ = false;
};

}
