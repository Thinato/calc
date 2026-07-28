#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

// A callable available to expressions. Adding a function is a single entry in
// the table in functions.cpp.
struct FunctionDef {
  std::string_view name;
  std::size_t arity = 1;
  Result<Value> (*apply)(const std::vector<Value>& args, std::size_t column) = nullptr;
};

// Returns nullptr when no function has that name.
const FunctionDef* find_function(std::string_view name);

}  // namespace calc
