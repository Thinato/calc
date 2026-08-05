#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/ast.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

struct FunctionDef {
  std::string_view name;
  std::size_t arity = 1;
  Result<Value> (*apply)(const std::vector<Value>& args, std::size_t column,
                         InfinityMode mode) = nullptr;
};

const FunctionDef* find_function(std::string_view name);

Result<Value> factorial(Value n, std::size_t column);

struct UserFunction {
  std::string name;
  std::vector<Param> params;
  std::vector<Statement> body;
  std::size_t defined_row = 0;
};

}
