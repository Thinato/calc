#pragma once

#include <cstddef>
#include <string>

#include "doc/document.hpp"

namespace calc {

enum class MotionKind {
  CharwiseExclusive,
  CharwiseInclusive,
  Linewise,
};

struct MotionResult {
  bool valid = false;
  Cursor target;
  MotionKind kind = MotionKind::CharwiseExclusive;
  bool clamped = false;
};

bool motion_needs_argument(char key);

bool is_motion(char key);

MotionResult apply_motion(const Document& document, Cursor from, char key, int count,
                          const std::string& argument, bool for_operator);

std::size_t first_non_blank(const Document& document, std::size_t row);

Cursor end_of_current_word(const Document& document, Cursor from, int count, bool big);

}
