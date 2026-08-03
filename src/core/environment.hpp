#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

struct Binding {
  Value value = 0;
  bool is_constant = false;
  std::size_t defined_row = 0;
  bool builtin = false;
};

class Environment {
 public:
  Environment();

  void reset();

  const Binding* find(std::string_view name) const;

  std::optional<Error> define(const std::string& name, Value value, std::size_t row,
                              std::size_t column);

  static bool is_constant_name(std::string_view name);

 private:
  std::map<std::string, Binding, std::less<>> bindings_;
};

}
