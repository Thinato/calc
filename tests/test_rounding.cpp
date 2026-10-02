#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/engine.hpp"

using namespace calc;

namespace {

std::string shown(std::string_view line) { return evaluate_line(line).text; }

}

TEST_CASE("floor rounds down, toward negative infinity") {
  CHECK(shown("floor(2.7)") == "2");
  CHECK(shown("floor(-2.7)") == "-3");
  CHECK(shown("floor(7 / 2)") == "3");
  CHECK(shown("floor(-7 / 2)") == "-4");
  CHECK(shown("floor(5)") == "5");
}

TEST_CASE("ceil rounds up, toward positive infinity") {
  CHECK(shown("ceil(2.1)") == "3");
  CHECK(shown("ceil(-2.7)") == "-2");
  CHECK(shown("ceil(7 / 2)") == "4");
  CHECK(shown("ceil(5)") == "5");
}

TEST_CASE("round goes to the nearest whole number, halves away from zero") {
  CHECK(shown("round(2.4)") == "2");
  CHECK(shown("round(2.6)") == "3");
  CHECK(shown("round(2.5)") == "3");
  CHECK(shown("round(-2.5)") == "-3");
  CHECK(shown("round(-2.4)") == "-2");
}

TEST_CASE("rounding leaves infinity alone") {
  CHECK(shown("floor(inf)") == "inf");
  CHECK(shown("ceil(-inf)") == "-inf");
  CHECK(shown("round(inf)") == "inf");
}

TEST_CASE("rounding functions are closures like any built-in") {
  CHECK(shown("sum(1, 3, round)") == "6");
  CHECK(shown("floor(sqrt(10))") == "3");
}

TEST_CASE("rounding functions take exactly one argument") {
  const LineEval outcome = evaluate_line("floor(1, 2)");
  CHECK(outcome.error.has_value());
}
