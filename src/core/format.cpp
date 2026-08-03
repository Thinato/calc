#include "core/format.hpp"

#include <cmath>
#include <cstdio>

namespace calc {

std::string format_value(Value value) {
  if (!std::isfinite(value)) return {};
  if (value == 0) return "0";

  char buffer[64];
  std::snprintf(buffer, sizeof buffer, "%.12g", value);
  std::string text(buffer);

  const std::size_t exponent = text.find('e');
  std::string mantissa = text.substr(0, exponent);
  const std::string suffix =
      exponent == std::string::npos ? std::string() : text.substr(exponent);

  if (mantissa.find('.') != std::string::npos) {
    mantissa.erase(mantissa.find_last_not_of('0') + 1);
    if (!mantissa.empty() && mantissa.back() == '.') mantissa.pop_back();
  }
  return mantissa + suffix;
}

}
