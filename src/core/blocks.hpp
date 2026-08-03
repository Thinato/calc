#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace calc {

struct Unit {
  std::size_t first_row = 0;
  std::size_t row_count = 1;
};

std::vector<Unit> scan_units(const std::vector<std::string>& lines);

std::string join_unit(const std::vector<std::string>& lines, Unit unit);

std::size_t row_for_column(const std::vector<std::string>& lines, Unit unit,
                           std::size_t column);

}
