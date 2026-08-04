#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/ast.hpp"
#include "core/engine.hpp"
#include "core/eval.hpp"
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

std::string square_def() { return "define f(x): x ^ 2\n"; }
std::string add_def() { return "define f(x, y): x + y\n"; }

}

TEST_CASE("a sum adds the closure over an inclusive range") {
  const Script script(square_def() + "sum(3, 6, f)\n");
  CHECK(script.shown(1) == "86");
}

TEST_CASE("nested sums walk a closure of two parameters") {
  const Script script(add_def() + "sum(1, 5, sum(1, 3, f))\n");
  CHECK(script.shown(1) == "75");
}

TEST_CASE("the outermost sum drives the first parameter") {
  const Script script("define f(x, y): x - y\nsum(1, 3, sum(1, 2, f))\n");
  CHECK(script.shown(1) == "3");
}

TEST_CASE("a sum is a value like any other") {
  SUBCASE("it composes with arithmetic") {
    const Script script("define f(x): x\n2 * sum(1, 3, f) + 1\n");
    CHECK(script.shown(1) == "13");
  }
  SUBCASE("its bounds are expressions, not just literals") {
    const Script script("define f(x): x\nn = 3\nsum(1, n * 2, f)\n");
    CHECK(script.shown(2) == "21");
  }
  SUBCASE("it flows into a name") {
    const Script script("define f(x): x\ntotal = sum(1, 4, f)\ntotal * 2\n");
    CHECK(script.shown(1) == "10");
    CHECK(script.shown(2) == "20");
  }
}

TEST_CASE("the range ends are both included") {
  const Script script(square_def() + "sum(3, 3, f)\n");
  CHECK(script.shown(1) == "9");
}

TEST_CASE("a reversed range is the empty sum, and never runs the closure") {
  const Script script("define boom(x): sqrt(-1)\nsum(6, 3, boom)\n");
  CHECK_FALSE(script.failed(1));
  CHECK(script.shown(1) == "0");
}

TEST_CASE("a range must be whole and finite") {
  SUBCASE("a fraction is refused, and quoted back") {
    const Script script(square_def() + "sum(1, 4.5, f)\n");
    CHECK(script.code(1) == ErrorCode::SumRange);
    CHECK(script.message(1) == "sum needs whole numbers, got 4.5");
  }
  SUBCASE("either end can be the fraction") {
    const Script script(square_def() + "sum(0.5, 4, f)\n");
    CHECK(script.message(1) == "sum needs whole numbers, got 0.5");
  }
  SUBCASE("infinity is refused before it is asked whether it is whole") {
    const Script script(square_def() + "sum(1, inf, f)\n");
    CHECK(script.code(1) == ErrorCode::SumRange);
    CHECK(script.message(1) == "sum needs a finite range");
  }
}

TEST_CASE("the shape of a sum is checked before anything runs") {
  SUBCASE("three arguments, no fewer") {
    const Script script(square_def() + "sum(1, 2)\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) ==
          "sum takes three arguments: a first value, a last value, and a function");
  }
  SUBCASE("and no more") {
    const Script script(square_def() + "sum(1, 2, f, 3)\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
  }
  SUBCASE("a closure with a spare parameter needs another sum around it") {
    const Script script(add_def() + "sum(3, 6, f)\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) ==
          "f() takes 2 arguments, so this sum needs one more sum around it");
  }
  SUBCASE("two spare parameters need two") {
    const Script script("define f(x, y, z): x\nsum(3, 6, f)\n");
    CHECK(script.message(1) ==
          "f() takes 3 arguments, so this sum needs 2 more sums around it");
  }
  SUBCASE("a sum that is already a number cannot be summed again") {
    const Script script(square_def() + "sum(1, 2, sum(1, 3, f))\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) == "a sum over f() is already a number");
  }
  SUBCASE("a closure needs at least one parameter") {
    const Script script("define answer(): 42\nsum(1, 3, answer)\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) ==
          "answer() takes no arguments, so there is nothing to sum over");
  }
}

TEST_CASE("the third argument names a function, and nothing else") {
  SUBCASE("not a call") {
    const Script script(square_def() + "sum(1, 3, f(2))\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) == "sum's third argument must be a function, not a call");
  }
  SUBCASE("not a number") {
    const Script script(square_def() + "sum(1, 3, 5)\n");
    CHECK(script.code(1) == ErrorCode::SumClosure);
    CHECK(script.message(1) == "sum's third argument must be a function");
  }
  SUBCASE("not a plain name") {
    const Script script("x = 2\nsum(1, 3, x)\n");
    CHECK(script.code(1) == ErrorCode::UnknownFunction);
    CHECK(script.message(1) == "unknown function 'x'");
  }
  SUBCASE("a name that is nothing at all") {
    const Script script("sum(1, 3, nope)\n");
    CHECK(script.code(0) == ErrorCode::UnknownFunction);
  }
}

TEST_CASE("a built-in function is a closure too") {
  SUBCASE("sqrt over a range") {
    const Script script("sum(1, 4, sqrt)\n");
    CHECK_FALSE(script.failed(0));
    CHECK(script.line(0).value == doctest::Approx(6.146264370));
  }
  SUBCASE("pow takes two, so it needs a nest") {
    CHECK(Script("sum(1, 3, pow)\n").code(0) == ErrorCode::SumClosure);
    CHECK(Script("sum(1, 2, sum(1, 3, pow))\n").shown(0) == "17");
  }
}

TEST_CASE("sum is a reserved word") {
  SUBCASE("it cannot hold a value") {
    const Script script("sum = 5\n");
    CHECK(script.code(0) == ErrorCode::AssignmentTarget);
    CHECK(script.message(0) == "the left of '=' must be a name");
  }
  SUBCASE("it cannot be defined") {
    const Script script("define sum(x): x\n");
    CHECK(script.code(0) == ErrorCode::DefineName);
    CHECK(script.message(0) == "expected a name after 'define'");
  }
  SUBCASE("it cannot be a parameter") {
    const Script script("define f(sum): sum\n");
    CHECK(script.code(0) == ErrorCode::DefineName);
    CHECK(script.message(0) == "expected a parameter name");
  }
  SUBCASE("it needs its parentheses") {
    const Script script("sum\n");
    CHECK(script.code(0) == ErrorCode::ExpectedCallParen);
    CHECK(script.message(0) == "expected '(' after 'sum'");
  }
}

TEST_CASE("an error inside the closure is named after the closure") {
  const Script script("define g(x): sqrt(2 - x)\nsum(1, 3, g)\n");
  CHECK(script.message(1) == "in g(): sqrt of a negative number");
}

TEST_CASE("a sum works inside a function body") {
  SUBCASE("with the bound taken from a parameter") {
    const Script script(square_def() + "define total(n): sum(1, n, f)\ntotal(3)\n");
    CHECK(script.shown(2) == "14");
  }
  SUBCASE("and across the rows of a braced body") {
    const Script script(square_def() +
                        "define total(n) {\n"
                        "  base = sum(1, n, f)\n"
                        "  return base * 2\n"
                        "}\n"
                        "total(3)\n");
    CHECK(script.shown(5) == "28");
  }
}

TEST_CASE("infinity travels through a sum") {
  SUBCASE("an infinite term makes the total infinite") {
    const Script script("define f(x): 1 / (x - 2)\nsum(1, 3, f)\n");
    CHECK(script.shown(1) == "inf");
  }
  SUBCASE("but finite terms that overflow are still an overflow") {
    const Script script("define f(x): 1e308\nsum(1, 3, f)\n");
    CHECK(script.code(1) == ErrorCode::NotFinite);
    CHECK(script.message(1) == "result is too large");
  }
}

TEST_CASE("a sum is bounded, so a huge range cannot freeze the buffer") {
  SUBCASE("one range over the limit") {
    const Script script(square_def() + "sum(1, 1000000000, f)\n");
    CHECK(script.code(1) == ErrorCode::SumRange);
    CHECK(script.message(1) == "sum has too many terms (limit 10000)");
  }
  SUBCASE("nested ranges are counted together, not one at a time") {
    const Script script(add_def() + "sum(1, 200, sum(1, 200, f))\n");
    CHECK(script.code(1) == ErrorCode::SumRange);
    CHECK(script.message(1) == "sum has too many terms (limit 10000)");
  }
  SUBCASE("but each line gets the whole budget to itself") {
    const Script script(square_def() + "sum(1, 9000, f)\nsum(1, 9000, f)\n");
    CHECK_FALSE(script.failed(1));
    CHECK_FALSE(script.failed(2));
  }
}

TEST_CASE("a function name is not a value") {
  const Environment environment;
  const NodePtr reference = make_node<FuncRef>(std::string("sqrt"), std::size_t{0});
  const Result<Value> result = evaluate(*reference, environment);
  REQUIRE_FALSE(result.ok());
  CHECK(result.error().code == ErrorCode::NameIsFunction);
  CHECK(result.error().message == "'sqrt' is a function, not a value");
}
