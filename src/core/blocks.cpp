#include "core/blocks.hpp"

#include <string_view>

#include "core/lexer.hpp"

namespace calc {
namespace {

constexpr std::string_view kDefine = "define";

std::string code_of(const std::string& line) {
  std::string code = line;
  const std::size_t comment = code.find('#');
  if (comment != std::string::npos) {
    code.replace(comment, std::string::npos, code.size() - comment, ' ');
  }
  return code;
}

bool starts_define(std::string_view code) {
  std::size_t index = 0;
  while (index < code.size() && (code[index] == ' ' || code[index] == '\t')) ++index;
  if (code.compare(index, kDefine.size(), kDefine) != 0) return false;
  const std::size_t after = index + kDefine.size();
  return after >= code.size() || !is_ident_continue(code[after]);
}

int brace_delta(std::string_view code) {
  int delta = 0;
  for (const char byte : code) {
    if (byte == '{') ++delta;
    if (byte == '}') --delta;
  }
  return delta;
}

}

std::vector<Unit> scan_units(const std::vector<std::string>& lines) {
  std::vector<Unit> units;

  std::size_t row = 0;
  while (row < lines.size()) {
    const std::string code = code_of(lines[row]);
    const int opened = brace_delta(code);

    if (opened <= 0 || !starts_define(code)) {
      units.push_back(Unit{row, 1});
      ++row;
      continue;
    }

    int depth = opened;
    std::size_t end = row;
    bool closed = false;
    for (std::size_t next = row + 1; next < lines.size(); ++next) {
      const std::string body = code_of(lines[next]);
      if (starts_define(body)) break;
      depth += brace_delta(body);
      if (depth <= 0) {
        end = next;
        closed = true;
        break;
      }
    }

    if (!closed) {
      units.push_back(Unit{row, 1});
      ++row;
      continue;
    }

    units.push_back(Unit{row, end - row + 1});
    row = end + 1;
  }

  return units;
}

std::string join_unit(const std::vector<std::string>& lines, Unit unit) {
  std::string joined;
  for (std::size_t offset = 0; offset < unit.row_count; ++offset) {
    if (offset > 0) joined += ';';
    joined += code_of(lines[unit.first_row + offset]);
  }
  return joined;
}

std::size_t row_for_column(const std::vector<std::string>& lines, Unit unit,
                           std::size_t column) {
  std::size_t consumed = 0;
  for (std::size_t offset = 0; offset + 1 < unit.row_count; ++offset) {
    consumed += lines[unit.first_row + offset].size() + 1;
    if (column < consumed) return unit.first_row + offset;
  }
  return unit.first_row + unit.row_count - 1;
}

}
