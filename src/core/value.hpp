#pragma once

namespace calc {

// The numeric type every expression evaluates to. Kept as an alias so the
// precision can be widened (long double, a decimal type) without touching the
// lexer, parser or evaluator.
using Value = double;

}  // namespace calc
