#pragma once

#include <string>

#include "core/value.hpp"

namespace calc {

// Renders a value the way a calculator should: "3" not "3.000000", and "0.3"
// not "0.30000000000000004". Formats at 12 significant digits, then trims
// trailing zeros. Returns an empty string for non-finite input, which callers
// treat as an error rather than printing "inf".
std::string format_value(Value value);

}  // namespace calc
