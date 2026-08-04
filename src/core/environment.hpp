#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/functions.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

inline constexpr std::size_t kMaxSumTerms = 10000;

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
  const UserFunction* find_user_function(std::string_view name) const;

  std::optional<Error> define(const std::string& name, Value value, std::size_t row,
                              std::size_t column);
  std::optional<Error> define_function(std::shared_ptr<const UserFunction> function,
                                       std::size_t column);

  Environment child_for_call() const;
  std::size_t depth() const { return depth_; }

  InfinityMode infinity_mode() const { return mode_; }
  void set_infinity_mode(InfinityMode mode) { mode_ = mode; }

  bool has_sum_budget() const { return sum_budget_ != nullptr; }
  Environment with_sum_budget() const;
  bool spend_sum_terms(std::size_t count) const;

  static bool is_constant_name(std::string_view name);
  static bool is_infinity_name(std::string_view name);

 private:
  std::map<std::string, Binding, std::less<>> bindings_;
  std::map<std::string, std::shared_ptr<const UserFunction>, std::less<>> functions_;
  std::size_t depth_ = 0;
  InfinityMode mode_ = InfinityMode::Signed;
  std::shared_ptr<std::size_t> sum_budget_;
};

}
