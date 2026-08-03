#include <cmath>
#include <limits>

#include <doctest/doctest.h>

#include "core/format.hpp"

using namespace calc;

TEST_CASE("format_value prints numbers the way a calculator should") {
  CHECK(format_value(3.0) == "3");
  CHECK(format_value(5.0) == "5");
  CHECK(format_value(1028.0) == "1028");
  CHECK(format_value(-7.0) == "-7");

  CHECK(format_value(0.1 + 0.2) == "0.3");
  CHECK(format_value(1.0 / 3.0) == "0.333333333333");

  CHECK(format_value(2.5) == "2.5");
  CHECK(format_value(0.5) == "0.5");
  CHECK(format_value(1.10) == "1.1");

  CHECK(format_value(0.0) == "0");
  CHECK(format_value(-0.0) == "0");

  CHECK(format_value(1e20) == "1e+20");
  CHECK(format_value(std::pow(2.0, 100)) == "1.26765060023e+30");
}

TEST_CASE("format_value refuses to print non-finite values") {
  CHECK(format_value(std::numeric_limits<double>::infinity()).empty());
  CHECK(format_value(-std::numeric_limits<double>::infinity()).empty());
  CHECK(format_value(std::numeric_limits<double>::quiet_NaN()).empty());
}
