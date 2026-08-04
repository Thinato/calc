#include "core/environment.hpp"

#include <algorithm>
#include <array>
#include <utility>

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
    BuiltinConstant{"inf", kInfinity},
    BuiltinConstant{"infinity", kInfinity},
};

}

Environment::Environment() { reset(); }

void Environment::reset() {
  bindings_.clear();
  functions_.clear();
  depth_ = 0;
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

const UserFunction* Environment::find_user_function(std::string_view name) const {
  const auto found = functions_.find(name);
  return found == functions_.end() ? nullptr : found->second.get();
}

Environment Environment::child_for_call() const {
  Environment child = *this;
  ++child.depth_;
  return child;
}

Environment Environment::with_sum_budget() const {
  Environment copy = *this;
  copy.sum_budget_ = std::make_shared<std::size_t>(kMaxSumTerms);
  return copy;
}

bool Environment::spend_sum_terms(std::size_t count) const {
  if (sum_budget_ == nullptr) return true;
  if (*sum_budget_ < count) return false;
  *sum_budget_ -= count;
  return true;
}

bool Environment::is_constant_name(std::string_view name) {
  return std::none_of(name.begin(), name.end(),
                      [](char byte) { return byte >= 'a' && byte <= 'z'; });
}

bool Environment::is_infinity_name(std::string_view name) {
  return name == "inf" || name == "infinity";
}

std::optional<Error> Environment::define(const std::string& name, Value value,
                                         std::size_t row, std::size_t column) {
  if (find_function(name) != nullptr || functions_.count(name) != 0) {
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

std::optional<Error> Environment::define_function(
    std::shared_ptr<const UserFunction> function, std::size_t column) {
  const std::string& name = function->name;

  if (find_function(name) != nullptr) {
    return make_error(ErrorCode::NameIsFunction, "'" + name + "' is a built-in function",
                      column);
  }

  const auto value = bindings_.find(name);
  if (value != bindings_.end()) {
    if (value->second.builtin) {
      return make_error(ErrorCode::ConstantReassigned, name + " is a built-in constant",
                        column);
    }
    return make_error(ErrorCode::FunctionRedefined,
                      "'" + name + "' is a name, defined on line " +
                          std::to_string(value->second.defined_row + 1),
                      column);
  }

  const auto existing = functions_.find(name);
  if (existing != functions_.end() && is_constant_name(name)) {
    return make_error(ErrorCode::FunctionRedefined,
                      name + " is a function, defined on line " +
                          std::to_string(existing->second->defined_row + 1),
                      column);
  }

  functions_[name] = std::move(function);
  return std::nullopt;
}

}
