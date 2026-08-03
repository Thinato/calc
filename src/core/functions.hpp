#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

struct FunctionDef {
  std::string_view name;
  std::size_t arity = 1;
  Result<Value> (*apply)(const std::vector<Value>& args, std::size_t column) = nullptr;
};

const FunctionDef* find_function(std::string_view name);

}
