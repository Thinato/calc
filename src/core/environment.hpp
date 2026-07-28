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
  std::size_t defined_row = 0;  // 0-based, so an error can name the line
  bool builtin = false;
};

// The names in effect at some point in the file.
//
// Whether a name is a variable or a constant is decided by its spelling alone
// (see is_constant_name), so the two can never disagree about which a given name
// is, and there is no separate registry to keep in sync.
class Environment {
 public:
  Environment();

  // Back to the built-in constants and nothing else. Called at the start of each
  // top-to-bottom pass over the document.
  void reset();

  const Binding* find(std::string_view name) const;

  // Defines or updates a name. Fails when the name belongs to a function, or
  // when it is a constant that already exists. On failure the existing binding
  // is left untouched, so lines below a rejected reassignment still see the
  // value the constant was first given.
  std::optional<Error> define(const std::string& name, Value value,
                              std::size_t row, std::size_t column);

  // True when the name holds no lowercase letter: PI, TAU, TEST_ONE. A name made
  // only of underscores counts as a constant, which is degenerate but harmless.
  static bool is_constant_name(std::string_view name);

 private:
  // std::less<> so string_view lookups need no temporary string.
  std::map<std::string, Binding, std::less<>> bindings_;
};

}  // namespace calc
