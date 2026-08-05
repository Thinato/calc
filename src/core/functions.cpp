#include "core/functions.hpp"

#include <array>
#include <cmath>
#include <string>

#include "core/format.hpp"

namespace calc {
namespace {

constexpr std::size_t kMaxFactorial = 170;

constexpr std::array<Value, kMaxFactorial + 1> make_factorials() {
  std::array<Value, kMaxFactorial + 1> table{};
  table[0] = 1;
  for (std::size_t n = 1; n <= kMaxFactorial; ++n) {
    table[n] = table[n - 1] * static_cast<Value>(n);
  }
  return table;
}

constexpr auto kFactorials = make_factorials();

Result<Value> apply_sqrt(const std::vector<Value>& args, std::size_t column,
                         InfinityMode) {
  if (args[0] < 0) {
    return make_error(ErrorCode::DomainError, "sqrt of a negative number", column);
  }
  return std::sqrt(args[0]);
}

Result<Value> apply_pow(const std::vector<Value>& args, std::size_t column,
                        InfinityMode mode) {
  if (args[0] == 0 && args[1] < 0) return infinity_for(mode, args[0]);

  const Value result = std::pow(args[0], args[1]);
  if (std::isnan(result) && !std::isnan(args[0]) && !std::isnan(args[1])) {
    return make_error(ErrorCode::DomainError, "pow is undefined here", column);
  }
  if (std::isinf(result) && std::isfinite(args[0]) && std::isfinite(args[1])) {
    return make_error(ErrorCode::NotFinite, "result is too large", column);
  }
  return result;
}

Result<Value> apply_fact(const std::vector<Value>& args, std::size_t column,
                         InfinityMode) {
  return factorial(args[0], column);
}

constexpr std::array kFunctions = {
    FunctionDef{"sqrt", 1, &apply_sqrt},
    FunctionDef{"pow", 2, &apply_pow},
    FunctionDef{"fact", 1, &apply_fact},
};

}

Result<Value> factorial(Value n, std::size_t column) {
  if (!std::isfinite(n)) {
    return make_error(ErrorCode::DomainError, "factorial needs a finite number", column);
  }
  if (n != std::floor(n)) {
    return make_error(ErrorCode::DomainError,
                      "factorial needs a whole number, got " + format_value(n), column);
  }
  if (n < 0) {
    return make_error(ErrorCode::DomainError, "factorial of a negative number", column);
  }
  if (n > static_cast<Value>(kMaxFactorial)) {
    return make_error(ErrorCode::NotFinite, "result is too large", column);
  }
  return kFactorials[static_cast<std::size_t>(n)];
}

const FunctionDef* find_function(std::string_view name) {
  for (const FunctionDef& def : kFunctions) {
    if (def.name == name) return &def;
  }
  return nullptr;
}

}
