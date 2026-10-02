#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/engine.hpp"
#include "core/lexer.hpp"
#include "doc/document.hpp"
#include "doc/results.hpp"

using namespace calc;

namespace {

LineEval evaluated(std::string_view line, InfinityMode mode = InfinityMode::Signed) {
  Environment environment;
  environment.set_infinity_mode(mode);
  return evaluate_line(line, environment, 0);
}

std::string shown(std::string_view line) { return evaluated(line).text; }

}

TEST_CASE("% lexes as its own token") {
  const auto tokens = tokenize("1 % 2");
  REQUIRE(tokens.ok());
  CHECK(tokens.value()[1].kind == TokenKind::Percent);
}

TEST_CASE("% is a floored remainder: the result has the sign of the divisor") {
  CHECK(shown("7 % 3") == "1");
  CHECK(shown("-7 % 3") == "2");
  CHECK(shown("7 % -3") == "-2");
  CHECK(shown("-7 % -3") == "-1");
  CHECK(shown("6 % 3") == "0");
  CHECK(shown("-6 % 3") == "0");
  CHECK(shown("7.5 % 2") == "1.5");
}

TEST_CASE("% binds like * and /") {
  CHECK(shown("1 + 7 % 3") == "2");
  CHECK(shown("2 * 7 % 3") == "2");
  CHECK(shown("7 % 3 * 2") == "2");
  CHECK(shown("2 ^ 3 % 5") == "3");
  CHECK(shown("5! % 7") == "1");
}

TEST_CASE("% by zero is an error in both infinity modes") {
  for (const InfinityMode mode : {InfinityMode::Signed, InfinityMode::Projective}) {
    for (std::string_view line : {"5 % 0", "0 % 0"}) {
      CAPTURE(line);
      const LineEval outcome = evaluated(line, mode);
      REQUIRE(outcome.error.has_value());
      CHECK(outcome.error->code == ErrorCode::DivisionByZero);
      CHECK(outcome.error->message == "modulo by zero");
      CHECK(outcome.error->column == 2);
    }
  }
}

TEST_CASE("% with infinity") {
  const LineEval undefined = evaluated("inf % 2");
  REQUIRE(undefined.error.has_value());
  CHECK(undefined.error->code == ErrorCode::DomainError);

  CHECK(shown("3 % inf") == "3");
  CHECK(shown("-3 % inf") == "inf");
}

TEST_CASE("% works inside a function body") {
  Document document = Document::from_text("define f(x): x % 2\nf(5)\nf(-5)");
  ResultCache results;
  results.refresh(document);
  CHECK(results.at(1).text == "1");
  CHECK(results.at(2).text == "1");
}
