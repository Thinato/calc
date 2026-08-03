#pragma once

#include "core/ast.hpp"
#include "core/environment.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

Result<Value> evaluate(const Node& node, const Environment& environment);

}
