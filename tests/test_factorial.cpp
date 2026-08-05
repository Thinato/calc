#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/engine.hpp"
#include "doc/document.hpp"
#include "doc/results.hpp"

using namespace calc;

namespace {

class Script {
 public:
  explicit Script(std::string_view text) : document_(Document::from_text(text)) {
    results_.refresh(document_);
  }

  const LineEval& line(std::size_t row) const { return results_.at(row); }
  std::string shown(std::size_t row) const { return results_.at(row).text; }

  bool failed(std::size_t row) const { return results_.at(row).error.has_value(); }
  ErrorCode code(std::size_t row) const {
    REQUIRE(results_.at(row).error.has_value());
    return results_.at(row).error->code;
  }
  std::string message(std::size_t row) const {
    REQUIRE(results_.at(row).error.has_value());
    return results_.at(row).error->message;
  }

 private:
  Document document_;
  ResultCache results_;
};

}

TEST_CASE("a factorial multiplies the whole numbers up to it") {
  const Script script("4!\n0!\n1!\n");
  CHECK(script.shown(0) == "24");
  CHECK(script.shown(1) == "1");
  CHECK(script.shown(2) == "1");
}

TEST_CASE("every factorial a double holds exactly comes back exactly") {
  const std::array<Value, 23> expected = {1.0,
                                          1.0,
                                          2.0,
                                          6.0,
                                          24.0,
                                          120.0,
                                          720.0,
                                          5040.0,
                                          40320.0,
                                          362880.0,
                                          3628800.0,
                                          39916800.0,
                                          479001600.0,
                                          6227020800.0,
                                          87178291200.0,
                                          1307674368000.0,
                                          20922789888000.0,
                                          355687428096000.0,
                                          6402373705728000.0,
                                          121645100408832000.0,
                                          2432902008176640000.0,
                                          51090942171709440000.0,
                                          1124000727777607680000.0};

  std::string text;
  for (std::size_t n = 0; n < expected.size(); ++n) text += std::to_string(n) + "!\n";
  const Script script(text);

  for (std::size_t n = 0; n < expected.size(); ++n) {
    CHECK_FALSE(script.failed(n));
    CHECK(script.line(n).value == expected[n]);
  }
  CHECK(script.shown(22) == "1.12400072778e+21");
}

TEST_CASE("past the exact range every shown digit is still right") {
  const Script script("25!\n170!\n");
  CHECK(script.shown(0) == "1.55112100433e+25");
  CHECK(script.shown(1) == "7.25741561531e+306");
}

TEST_CASE("the range has an end") {
  const Script script("171!\n");
  CHECK(script.code(0) == ErrorCode::NotFinite);
  CHECK(script.message(0) == "result is too large");
}

TEST_CASE("a factorial needs a whole number that is not negative") {
  SUBCASE("a fraction is refused, and quoted back") {
    const Script script("4.5!\n");
    CHECK(script.code(0) == ErrorCode::DomainError);
    CHECK(script.message(0) == "factorial needs a whole number, got 4.5");
  }
  SUBCASE("infinity is refused before it is asked whether it is whole") {
    const Script script("inf!\n");
    CHECK(script.code(0) == ErrorCode::DomainError);
    CHECK(script.message(0) == "factorial needs a finite number");
  }
  SUBCASE("a negative number has no factorial") {
    const Script script("(-1)!\n");
    CHECK(script.code(0) == ErrorCode::DomainError);
    CHECK(script.message(0) == "factorial of a negative number");
  }
  SUBCASE("a negative fraction is named a fraction first") {
    const Script script("(-2.5)!\n");
    CHECK(script.message(0) == "factorial needs a whole number, got -2.5");
  }
}

TEST_CASE("'!' binds tighter than every other operator") {
  const Script script("2^3!\n3!^2\n-3!\n2 * 3!\n(2 + 2)!\nsqrt(4)!\n");
  CHECK(script.shown(0) == "64");
  CHECK(script.shown(1) == "36");
  CHECK(script.shown(2) == "-6");
  CHECK(script.shown(3) == "12");
  CHECK(script.shown(4) == "24");
  CHECK(script.shown(5) == "2");
}

TEST_CASE("so a minus sign in front is not part of the number") {
  const Script script("-1!\n");
  CHECK_FALSE(script.failed(0));
  CHECK(script.shown(0) == "-1");
}

TEST_CASE("factorials chain") {
  const Script script("3!!\n5!!\n6!!\n");
  CHECK(script.shown(0) == "720");
  CHECK(script.shown(1) == "6.68950291345e+198");
  CHECK(script.code(2) == ErrorCode::NotFinite);
}

TEST_CASE("'!' is a suffix, and only a suffix") {
  const Script script("!3\n");
  CHECK(script.code(0) == ErrorCode::UnexpectedToken);
  CHECK(script.message(0) == "unexpected '!'");
}

TEST_CASE("a factorial is a value like any other") {
  SUBCASE("it reads a name") {
    const Script script("n = 4\nn!\n");
    CHECK(script.shown(1) == "24");
  }
  SUBCASE("it reads a parameter") {
    const Script script("define f(n): n! / 2\nf(5)\n");
    CHECK(script.shown(1) == "60");
  }
  SUBCASE("and it works across the rows of a braced body") {
    const Script script("define g(n) {\n  base = n!\n  return base + 1\n}\ng(4)\n");
    CHECK(script.shown(4) == "25");
  }
}

TEST_CASE("an assigned factorial shows its answer, unlike an assigned number") {
  const Script script("a = 4!\nb = 4\nc = -4\n");
  CHECK(script.line(0).show_result);
  CHECK_FALSE(script.line(1).show_result);
  CHECK_FALSE(script.line(2).show_result);
}

TEST_CASE("fact() is the same table under a name") {
  SUBCASE("it agrees with '!'") {
    const Script script("fact(4)\nfact(20) - 20!\n");
    CHECK(script.shown(0) == "24");
    CHECK(script.shown(1) == "0");
  }
  SUBCASE("it takes one argument") {
    const Script script("fact(1, 2)\n");
    CHECK(script.code(0) == ErrorCode::WrongArity);
    CHECK(script.message(0) == "fact() takes 1 argument, got 2");
  }
  SUBCASE("it reports its own domain the same way") {
    const Script script("fact(-1)\n");
    CHECK(script.message(0) == "factorial of a negative number");
  }
  SUBCASE("and the name is protected like any built-in") {
    CHECK(Script("fact = 5\n").code(0) == ErrorCode::NameIsFunction);
    CHECK(Script("define fact(x): x\n").code(0) == ErrorCode::NameIsFunction);
  }
}

TEST_CASE("fact() can be summed over, which '!' cannot") {
  SUBCASE("as a closure") {
    const Script script("sum(1, 5, fact)\n");
    CHECK(script.shown(0) == "153");
  }
  SUBCASE("it takes one argument, so it needs no nest") {
    const Script script("sum(1, 3, sum(1, 3, fact))\n");
    CHECK(script.code(0) == ErrorCode::SumClosure);
    CHECK(script.message(0) == "a sum over fact() is already a number");
  }
  SUBCASE("and '!' inside a closure body reaches the same table") {
    const Script script("define f(x): x!\nsum(1, 5, f)\n");
    CHECK(script.shown(1) == "153");
  }
}
