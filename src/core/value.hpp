#pragma once

#include <cmath>
#include <limits>

namespace calc {

using Value = double;

inline constexpr Value kInfinity = std::numeric_limits<Value>::infinity();

enum class InfinityMode { Signed, Projective };

inline Value infinity_for(InfinityMode mode, Value sign_source) {
  if (mode == InfinityMode::Projective) return kInfinity;
  return std::copysign(kInfinity, sign_source);
}

inline Value normalized(Value value, InfinityMode mode) {
  return std::isinf(value) ? infinity_for(mode, value) : value;
}

}
