#include "core/environment.hpp"

#include <algorithm>
#include <array>

#include "core/functions.hpp"

namespace calc {
namespace {

struct BuiltinConstant {
  std::string_view name;
  Value value;
};

constexpr std::array kBuiltins = {
    BuiltinConstant{"PI", 3.14159265358979323846},
    BuiltinConstant{"E", 2.71828182845904523536},
    BuiltinConstant{"TAU", 6.28318530717958647692},
};

}

Environment::Environment() { reset(); }

void Environment::reset() {
  bindings_.clear();
  for (const BuiltinConstant& builtin : kBuiltins) {
    Binding binding;
    binding.value = builtin.value;
    binding.is_constant = true;
    binding.builtin = true;
    bindings_.emplace(std::string(builtin.name), binding);
  }
}

const Binding* Environment::find(std::string_view name) const {
  const auto found = bindings_.find(name);
  return found == bindings_.end() ? nullptr : &found->second;
}

bool Environment::is_constant_name(std::string_view name) {
  return std::none_of(name.begin(), name.end(),
                      [](char byte) { return byte >= 'a' && byte <= 'z'; });
}

std::optional<Error> Environment::define(const std::string& name, Value value,
                                         std::size_t row, std::size_t column) {
  if (find_function(name) != nullptr) {
    return make_error(ErrorCode::NameIsFunction, "'" + name + "' is a function", column);
  }

  const auto existing = bindings_.find(name);
  if (existing != bindings_.end() && existing->second.is_constant) {
    if (existing->second.builtin) {
      return make_error(ErrorCode::ConstantReassigned, name + " is a built-in constant",
                        column);
    }
    return make_error(ErrorCode::ConstantReassigned,
                      name + " is a constant, defined on line " +
                          std::to_string(existing->second.defined_row + 1),
                      column);
  }

  Binding& binding = bindings_[name];
  binding.value = value;
  binding.is_constant = is_constant_name(name);
  binding.defined_row = row;
  binding.builtin = false;
  return std::nullopt;
}

}
