#include "core/functions.hpp"

#include <array>
#include <cmath>

namespace calc {
namespace {

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

constexpr std::array kFunctions = {
    FunctionDef{"sqrt", 1, &apply_sqrt},
    FunctionDef{"pow", 2, &apply_pow},
};

}

const FunctionDef* find_function(std::string_view name) {
  for (const FunctionDef& def : kFunctions) {
    if (def.name == name) return &def;
  }
  return nullptr;
}

}
