#pragma once

#include "core/ast.hpp"
#include "core/environment.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

// Evaluates an expression. Names are resolved against `environment`, so a name
// the file has not defined yet is an error rather than a zero.
Result<Value> evaluate(const Node& node, const Environment& environment);

}  // namespace calc
