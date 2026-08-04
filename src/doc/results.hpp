#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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

  bool is_function_at(std::string_view name, std::size_t row) const;

  std::optional<std::string> definition_at(std::size_t row) const;

 private:
  std::vector<LineEval> entries_;
  Environment environment_;
  std::map<std::string, std::size_t, std::less<>> function_rows_;
  std::vector<std::optional<std::string>> definition_rows_;
  std::size_t revision_ = 0;
  bool primed_ = false;
};

}
