#include <doctest/doctest.h>

#include <string_view>

#include "core/parser.hpp"

using namespace calc;

namespace {

ErrorCode error_code_for(std::string_view line) {
  const auto tree = parse(line);
  REQUIRE_FALSE(tree.ok());
  return tree.error().code;
}

std::size_t error_column_for(std::string_view line) {
  const auto tree = parse(line);
  REQUIRE_FALSE(tree.ok());
  return tree.error().column;
}

}  // namespace

TEST_CASE("parser accepts every operator in the language") {
  for (std::string_view line : {"1 + 2", "1 - 2", "1 * 2", "1 / 2", "1 ^ 2",
                                "(1 + 2) * 3", "-4", "+4", "sqrt(16)",
                                "pow(2, 10)", "sqrt(pow(3, 2))", ".5 + 1"}) {
    CAPTURE(line);
    CHECK(parse(line).ok());
  }
}

TEST_CASE("parser reports incomplete expressions") {
  CHECK(error_code_for("1 +") == ErrorCode::UnexpectedEnd);
  CHECK(error_code_for("") == ErrorCode::UnexpectedEnd);
  CHECK(error_code_for("*") == ErrorCode::UnexpectedToken);
}

TEST_CASE("parser reports unbalanced parentheses at the opening paren") {
  CHECK(error_code_for("(1 + 2") == ErrorCode::UnbalancedParen);
  CHECK(error_column_for("2 * (1 + 3") == 4);

  // A stray closer is trailing junk, not an unbalanced open.
  CHECK(error_code_for("1 + 2)") == ErrorCode::UnexpectedToken);
}

TEST_CASE("parser rejects trailing junk") {
  CHECK(error_code_for("1 2") == ErrorCode::UnexpectedToken);
  CHECK(error_column_for("1 2") == 2);
}

TEST_CASE("parser validates function names and arity at parse time") {
  SUBCASE("unknown name") {
    CHECK(error_code_for("sin(1)") == ErrorCode::UnknownFunction);
    CHECK(error_column_for("1 + sin(1)") == 4);
  }
  SUBCASE("missing call parentheses") {
    CHECK(error_code_for("sqrt 16") == ErrorCode::ExpectedCallParen);
  }
  SUBCASE("too few arguments") {
    CHECK(error_code_for("pow(2)") == ErrorCode::WrongArity);
  }
  SUBCASE("too many arguments") {
    CHECK(error_code_for("sqrt(2, 3)") == ErrorCode::WrongArity);
  }
  SUBCASE("no arguments at all") {
    CHECK(error_code_for("sqrt()") == ErrorCode::WrongArity);
  }
}

TEST_CASE("parser keeps the comment out of the expression") {
  CHECK(parse("1 + 2 # a note").ok());
}
