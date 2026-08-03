#pragma once

#include <cstddef>
#include <string>

#include "doc/document.hpp"
#include "vim/motions.hpp"

namespace calc {

enum class Operator { None, Delete, Change, Yank };

struct Register {
  std::string text;
  bool linewise = false;
};

struct Range {
  bool linewise = false;
  Cursor from;
  Cursor to;
  std::size_t first_row = 0;
  std::size_t row_count = 0;
};

Range resolve_range(const Document& document, Cursor start, const MotionResult& motion);

}
