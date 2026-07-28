#pragma once

#include <cstddef>
#include <string>

#include "doc/document.hpp"

namespace calc {

// How a motion's span is measured when an operator consumes it.
enum class MotionKind {
  CharwiseExclusive,  // up to, but not including, the target character
  CharwiseInclusive,  // up to and including the target character
  Linewise,           // whole lines between the start and target rows
};

struct MotionResult {
  bool valid = false;
  Cursor target;
  MotionKind kind = MotionKind::CharwiseExclusive;
  // True for motions that must not fail an operator when they cannot move the
  // full count, e.g. `d$` on the last line.
  bool clamped = false;
};

// True for f, F, t and T, which need the next typed character before they can
// run.
bool motion_needs_argument(char key);

// True when `key` names a motion at all.
bool is_motion(char key);

// Runs a motion. `count` is at least 1. `argument` is the character typed after
// f/F/t/T and is otherwise ignored. `for_operator` selects vim's operator-
// pending behaviour where it differs from plain cursor movement.
MotionResult apply_motion(const Document& document, Cursor from, char key,
                          int count, const std::string& argument,
                          bool for_operator);

// Column of the first non-blank character, or 0 for a blank line.
std::size_t first_non_blank(const Document& document, std::size_t row);

// End of the run of same-class characters the cursor sits in. This is what `cw`
// operates on: vim changes to the end of the current word only, and never
// reaches into the next one the way `w` and `e` do. On a one-character word
// `cw` therefore touches just that character.
Cursor end_of_current_word(const Document& document, Cursor from, int count, bool big);

}  // namespace calc
