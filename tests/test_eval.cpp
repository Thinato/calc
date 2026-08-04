#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/engine.hpp"

using namespace calc;

namespace {

std::string shown(std::string_view line) {
  const LineEval outcome = evaluate_line(line);
  return outcome.text;
}

ErrorCode error_code_for(std::string_view line) {
  const LineEval outcome = evaluate_line(line);
  REQUIRE(outcome.error.has_value());
  return outcome.error->code;
}

}

TEST_CASE("the examples from the brief") {
  CHECK(shown("1 + 2") == "3");
  CHECK(shown("sqrt(16) + pow(2, 10)") == "1028");
  CHECK(shown("(1 + 2) * 3 ^ 2") == "27");
}

TEST_CASE("every symbol in the language") {
  CHECK(shown("2 + 3") == "5");
  CHECK(shown("7 - 2") == "5");
  CHECK(shown("6 * 7") == "42");
  CHECK(shown("84 / 2") == "42");
  CHECK(shown("2 ^ 10") == "1024");
  CHECK(shown("(2 + 3) * 4") == "20");
  CHECK(shown("pow(2, 10)") == "1024");
  CHECK(shown("sqrt(144)") == "12");
}

TEST_CASE("precedence and associativity") {
  SUBCASE("multiplication binds tighter than addition") {
    CHECK(shown("1 + 2 * 3") == "7");
    CHECK(shown("2 * 3 + 1") == "7");
  }
  SUBCASE("power binds tighter than multiplication") {
    CHECK(shown("2 * 3 ^ 2") == "18");
  }
  SUBCASE("power is right associative") { CHECK(shown("2 ^ 3 ^ 2") == "512"); }
  SUBCASE("addition and subtraction are left associative") {
    CHECK(shown("1 - 2 - 3") == "-4");
    CHECK(shown("100 / 10 / 2") == "5");
  }
  SUBCASE("unary minus binds looser than power, as in mathematics") {
    CHECK(shown("-2 ^ 2") == "-4");
  }
  SUBCASE("a power may take a negative exponent") {
    CHECK(shown("2 ^ -1") == "0.5");
    CHECK(shown("2 ^ -2") == "0.25");
  }
  SUBCASE("parentheses override precedence") {
    CHECK(shown("(-2) ^ 2") == "4");
    CHECK(shown("(1 + 2) * 3") == "9");
  }
  SUBCASE("stacked unary operators") {
    CHECK(shown("--5") == "5");
    CHECK(shown("-+-5") == "5");
  }
}

TEST_CASE("nesting and whitespace") {
  CHECK(shown("sqrt(pow(3, 2))") == "3");
  CHECK(shown("pow(1 + 1, 2 + 2)") == "16");
  CHECK(shown("1+2") == "3");
  CHECK(shown("   1   +   2   ") == "3");
  CHECK(shown("1 + 2 # a trailing note") == "3");
}

TEST_CASE("blank and comment lines produce neither result nor error") {
  for (std::string_view line : {"", "    ", "# heading", "  # indented"}) {
    CAPTURE(line);
    const LineEval outcome = evaluate_line(line);
    CHECK_FALSE(outcome.has_result());
    CHECK_FALSE(outcome.error.has_value());
  }
}

TEST_CASE("a failed line yields an error and no result") {
  const LineEval outcome = evaluate_line("1 +");
  CHECK_FALSE(outcome.has_result());
  CHECK(outcome.text.empty());
  REQUIRE(outcome.error.has_value());
}

TEST_CASE("arithmetic errors") {
  SUBCASE("only zero divided by zero is an error") {
    CHECK(error_code_for("0 / 0") == ErrorCode::DivisionByZero);
    CHECK(error_code_for("(2 - 2) / (2 - 2)") == ErrorCode::DivisionByZero);
    CHECK(shown("1 / 0") == "inf");
    CHECK(shown("1 / (2 - 2)") == "inf");
  }
  SUBCASE("square root of a negative number") {
    CHECK(error_code_for("sqrt(-1)") == ErrorCode::DomainError);
  }
  SUBCASE("a fractional power of a negative base is undefined") {
    CHECK(error_code_for("pow(-8, 0.5)") == ErrorCode::DomainError);
    CHECK(error_code_for("(-8) ^ 0.5") == ErrorCode::DomainError);
  }
  SUBCASE("overflow is reported rather than shown as inf") {
    CHECK(error_code_for("2 ^ 100000") == ErrorCode::NotFinite);
    CHECK(error_code_for("pow(10, 400)") == ErrorCode::NotFinite);
  }
}

TEST_CASE("error columns point at the offending character") {
  const LineEval outcome = evaluate_line("10 + 0 / 0");
  REQUIRE(outcome.error.has_value());
  CHECK(outcome.error->column == 7);
}

TEST_CASE("results round-trip through the display format") {
  for (std::string_view line : {"2 ^ 100", "1 / 3", "0.1 + 0.2", "1 + 2"}) {
    CAPTURE(line);
    const LineEval first = evaluate_line(line);
    REQUIRE(first.has_result());
    const LineEval second = evaluate_line(first.text);
    REQUIRE(second.has_result());
    CHECK(second.text == first.text);
  }
}
