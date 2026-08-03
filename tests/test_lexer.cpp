#include <doctest/doctest.h>

#include "core/lexer.hpp"

using namespace calc;

TEST_CASE("blank and comment lines carry nothing to evaluate") {
  CHECK(is_blank_or_comment(""));
  CHECK(is_blank_or_comment("   \t "));
  CHECK(is_blank_or_comment("# groceries"));
  CHECK(is_blank_or_comment("   # indented comment"));
  CHECK_FALSE(is_blank_or_comment("1 + 2"));
  CHECK_FALSE(is_blank_or_comment("1 + 2 # with a trailing comment"));
}

TEST_CASE("tokenize splits operators and numbers") {
  const auto tokens = tokenize("1 + 2.5 * (3 - 4) / 5 ^ 6");
  REQUIRE(tokens.ok());
  const auto& list = tokens.value();
  REQUIRE(list.size() == 14);
  CHECK(list[0].kind == TokenKind::Number);
  CHECK(list[0].number == doctest::Approx(1.0));
  CHECK(list[1].kind == TokenKind::Plus);
  CHECK(list[2].number == doctest::Approx(2.5));
  CHECK(list[3].kind == TokenKind::Star);
  CHECK(list[4].kind == TokenKind::LParen);
  CHECK(list[8].kind == TokenKind::RParen);
  CHECK(list[9].kind == TokenKind::Slash);
  CHECK(list[11].kind == TokenKind::Caret);
  CHECK(list.back().kind == TokenKind::End);
}

TEST_CASE("tokenize records columns for error reporting") {
  const auto tokens = tokenize("12 + 34");
  REQUIRE(tokens.ok());
  CHECK(tokens.value()[0].column == 0);
  CHECK(tokens.value()[1].column == 3);
  CHECK(tokens.value()[2].column == 5);
}

TEST_CASE("tokenize reads identifiers as names") {
  const auto tokens = tokenize("sqrt(16)");
  REQUIRE(tokens.ok());
  CHECK(tokens.value()[0].kind == TokenKind::Identifier);
  CHECK(tokens.value()[0].text == "sqrt");
}

TEST_CASE("tokenize accepts leading-dot and exponent numbers") {
  SUBCASE("leading dot") {
    const auto tokens = tokenize(".5");
    REQUIRE(tokens.ok());
    CHECK(tokens.value()[0].number == doctest::Approx(0.5));
  }
  SUBCASE("exponent, so printed results can be pasted back") {
    const auto tokens = tokenize("1.26765060023e+30");
    REQUIRE(tokens.ok());
    CHECK(tokens.value()[0].kind == TokenKind::Number);
    CHECK(tokens.value()[0].number == doctest::Approx(1.26765060023e+30));
  }
  SUBCASE("a digit touching a letter is a malformed name, not two tokens") {
    const auto tokens = tokenize("2e");
    REQUIRE_FALSE(tokens.ok());
    CHECK(tokens.error().code == ErrorCode::InvalidName);
  }
}

TEST_CASE("tokenize stops at a comment") {
  const auto tokens = tokenize("1 + 2 # ignored ($$$)");
  REQUIRE(tokens.ok());
  REQUIRE(tokens.value().size() == 4);
  CHECK(tokens.value()[3].kind == TokenKind::End);
}

TEST_CASE("tokenize rejects stray characters and points at them") {
  const auto tokens = tokenize("1 + $");
  REQUIRE_FALSE(tokens.ok());
  CHECK(tokens.error().code == ErrorCode::UnexpectedCharacter);
  CHECK(tokens.error().column == 4);
}
