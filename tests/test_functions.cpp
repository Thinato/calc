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
  std::size_t column(std::size_t row) const {
    REQUIRE(results_.at(row).error.has_value());
    return results_.at(row).error->column;
  }
  bool is_function_at(std::string_view name, std::size_t row) const {
    return results_.is_function_at(name, row);
  }

 private:
  Document document_;
  ResultCache results_;
};

ErrorCode code_for(std::string_view line) {
  const LineEval outcome = evaluate_line(line);
  REQUIRE(outcome.error.has_value());
  return outcome.error->code;
}

}

TEST_CASE("the three spellings define the same function") {
  const Script script(
      "define a(x): x ^ 2\n"
      "define b(x) { return x ^ 2 }\n"
      "define c(x) {\n"
      "  y = x ^ 2\n"
      "  return y\n"
      "}\n"
      "a(3)\n"
      "b(3)\n"
      "c(3)\n");

  CHECK(script.shown(6) == "9");
  CHECK(script.shown(7) == "9");
  CHECK(script.shown(8) == "9");
}

TEST_CASE("the value of a body is its last statement, with or without 'return'") {
  const Script script(
      "define with(x) { y = x * 2; return y }\n"
      "define without(x) { y = x * 2; y }\n"
      "define assigned(x) { y = x * 2 }\n"
      "with(4)\n"
      "without(4)\n"
      "assigned(4)\n");

  CHECK(script.shown(3) == "8");
  CHECK(script.shown(4) == "8");
  CHECK(script.shown(5) == "8");
}

TEST_CASE("a definition is not a value, so nothing is shown after it") {
  const Script script("define f(x): x ^ 2\n");

  CHECK_FALSE(script.line(0).has_result());
  CHECK_FALSE(script.failed(0));
  CHECK(script.line(0).is_definition());
  CHECK(script.line(0).defined_name == std::string("f"));
  CHECK(script.line(0).defined_column == 7);
}

TEST_CASE("the worked example from the brief") {
  const Script script(
      "define f(x, y) {\n"
      "  z = sqrt(x^2 + y^2)\n"
      "  return z\n"
      "}\n"
      "f(3, 4)\n");

  CHECK(script.shown(4) == "5");
}

TEST_CASE("parameters shadow the document's names") {
  const Script script(
      "x = 100\n"
      "define double(x): x * 2\n"
      "double(3)\n"
      "x\n");

  CHECK(script.shown(2) == "6");
  CHECK(script.shown(3) == "100");
}

TEST_CASE("a body resolves names where it is called") {
  const Script script(
      "define f(x): x * k\n"
      "k = 2\n"
      "f(3)\n");

  CHECK(script.failed(0) == false);
  CHECK(script.shown(2) == "6");
}

TEST_CASE("a name the body cannot resolve is reported at the call") {
  const Script script(
      "define g(x): x + nope\n"
      "g(1)\n");

  CHECK_FALSE(script.failed(0));
  CHECK(script.code(1) == ErrorCode::UndefinedName);
  CHECK(script.message(1) == "in g(): undefined name 'nope'");
  CHECK(script.column(1) == 0);
}

TEST_CASE("only the innermost function is named, and the column is this row's") {
  const Script script(
      "define inner(x): (x - x) / 0\n"
      "define outer(x): inner(x)\n"
      "1 + outer(2)\n");

  CHECK(script.message(2) == "in inner(): division by zero");
  CHECK(script.column(2) == 4);
}

TEST_CASE("a call is checked for arity where it is written") {
  const Script script(
      "define f(x, y): x + y\n"
      "f(1)\n"
      "f(1, 2, 3)\n"
      "f(1, 2)\n");

  CHECK(script.code(1) == ErrorCode::WrongArity);
  CHECK(script.message(1) == "f() takes 2 arguments, got 1");
  CHECK(script.code(2) == ErrorCode::WrongArity);
  CHECK(script.shown(3) == "3");
}

TEST_CASE("a body may only call functions that already exist") {
  const Script script(
      "define f(x): later(x)\n"
      "define later(x): x\n");

  CHECK(script.code(0) == ErrorCode::UnknownFunction);
  CHECK(script.message(0) == "unknown function 'later'");
}

TEST_CASE("a function cannot call itself") {
  CHECK(code_for("define f(x): f(x)") == ErrorCode::UnknownFunction);
}

TEST_CASE("recursion reached by redefinition is stopped, not crashed") {
  const Script script(
      "define f(x): x\n"
      "define f(x): f(x)\n"
      "f(1)\n");

  CHECK(script.code(2) == ErrorCode::TooMuchRecursion);
  CHECK(script.message(2) == "too much recursion in 'f'");
}

TEST_CASE("a function takes no arguments if it says so") {
  const Script script(
      "define answer(): 42\n"
      "answer()\n"
      "answer\n");

  CHECK(script.shown(1) == "42");
  CHECK(script.code(2) == ErrorCode::ExpectedCallParen);
}

TEST_CASE("a definition is refused when the name is taken") {
  CHECK(code_for("define sqrt(x): x") == ErrorCode::NameIsFunction);
  CHECK(code_for("define PI(x): x") == ErrorCode::ConstantReassigned);

  const Script taken(
      "x = 1\n"
      "define x(a): a\n");
  CHECK(taken.code(1) == ErrorCode::FunctionRedefined);
  CHECK(taken.message(1) == "'x' is a name, defined on line 1");
}

TEST_CASE("a value cannot take a function's name") {
  const Script script(
      "define f(x): x\n"
      "f = 2\n");

  CHECK(script.code(1) == ErrorCode::NameIsFunction);
  CHECK(script.message(1) == "'f' is a function");
}

TEST_CASE("redefining a function follows the same spelling rule as a value") {
  const Script variable(
      "define f(x): x\n"
      "define f(x): x * 2\n"
      "f(3)\n");
  CHECK(variable.shown(2) == "6");

  const Script constant(
      "define F(x): x\n"
      "define F(x): x * 2\n");
  CHECK(constant.code(1) == ErrorCode::FunctionRedefined);
  CHECK(constant.message(1) == "F is a function, defined on line 1");
}

TEST_CASE("a body's own names follow the document's rules") {
  const Script script(
      "define f(x) { RATE = 1; RATE = 2; RATE }\n"
      "f(1)\n"
      "define g(PI): PI\n"
      "g(1)\n");

  CHECK(script.code(1) == ErrorCode::ConstantReassigned);
  CHECK(script.message(1) == "in f(): RATE is a constant, defined on line 1");
  CHECK(script.code(3) == ErrorCode::ConstantReassigned);
  CHECK(script.message(3) == "in g(): PI is a built-in constant");
}

TEST_CASE("what a definition may not contain") {
  CHECK(code_for("define f(x) { return x; x }") == ErrorCode::ReturnNotLast);
  CHECK(code_for("define f(x) {}") == ErrorCode::EmptyBody);
  CHECK(code_for("define f(x, x): x") == ErrorCode::DuplicateParameter);
  CHECK(code_for("define f(x) { define g(y): y; x }") == ErrorCode::UnexpectedToken);
  CHECK(code_for("define f(x) { x } y") == ErrorCode::UnexpectedToken);
}

TEST_CASE("a malformed definition says which part is missing") {
  CHECK(code_for("define") == ErrorCode::DefineName);
  CHECK(code_for("define = 5") == ErrorCode::DefineName);
  CHECK(code_for("define f: x") == ErrorCode::DefineName);
  CHECK(code_for("define f(x) x") == ErrorCode::DefineName);
  CHECK(code_for("define f(2): x") == ErrorCode::DefineName);
  CHECK(code_for("define f(x: x") == ErrorCode::UnbalancedParen);
  CHECK(code_for("define f(x) { x") == ErrorCode::UnbalancedParen);
  CHECK(code_for("return 2") == ErrorCode::ReturnOutsideBody);
}

TEST_CASE("the rows of a body carry neither result nor error") {
  const Script script(
      "define f(x) {\n"
      "  y = x * 2\n"
      "  return y\n"
      "}\n"
      "f(2)\n");

  for (std::size_t row = 1; row <= 3; ++row) {
    CAPTURE(row);
    CHECK_FALSE(script.line(row).has_result());
    CHECK_FALSE(script.failed(row));
  }
  CHECK(script.shown(4) == "4");
}

TEST_CASE("an error inside a body is reported on the row that caused it") {
  const Script script(
      "define f(x) {\n"
      "  y = x * 2\n"
      "  z = sqrt(1, 2)\n"
      "  return y\n"
      "}\n");

  CHECK_FALSE(script.failed(0));
  CHECK_FALSE(script.failed(1));
  CHECK(script.code(2) == ErrorCode::WrongArity);
  CHECK_FALSE(script.failed(3));
}

TEST_CASE("a body may contain blank and comment rows") {
  const Script script(
      "define f(x) {\n"
      "  # twice it\n"
      "\n"
      "  return x * 2\n"
      "}\n"
      "f(4)\n");

  CHECK(script.shown(5) == "8");
}

TEST_CASE("a '{' with no '}' leaves the rows below it working") {
  const Script script(
      "define f(x) {\n"
      "  y = x\n"
      "\n"
      "subtotal = 4\n"
      "subtotal * 2\n");

  CHECK(script.code(0) == ErrorCode::UnbalancedParen);
  CHECK(script.message(0) == "unclosed '{'");
  CHECK(script.shown(4) == "8");
}

TEST_CASE("a later definition's brace does not close an earlier one") {
  const Script script(
      "define f(x) {\n"
      "  x\n"
      "define g(y) { return y }\n"
      "g(3)\n");

  CHECK(script.code(0) == ErrorCode::UnbalancedParen);
  CHECK(script.shown(3) == "3");
}

TEST_CASE("a function is only a function from its definition down") {
  const Script script(
      "f(2)\n"
      "define f(x): x\n"
      "f(2)\n");

  CHECK(script.code(0) == ErrorCode::UnknownFunction);
  CHECK_FALSE(script.is_function_at("f", 0));
  CHECK(script.is_function_at("f", 1));
  CHECK(script.is_function_at("f", 2));
  CHECK_FALSE(script.is_function_at("nope", 2));
}

TEST_CASE("the examples in the README") {
  const Script script(
      "define double(x): x * 2\n"
      "double(21)\n"
      "define hyp(a, b) { squares = a^2 + b^2; return sqrt(squares) }\n"
      "hyp(3, 4)\n"
      "define area(r) {\n"
      "  PI * r^2\n"
      "}\n"
      "area(3)\n"
      "define with_tax(amount): amount * (1 + RATE)\n"
      "RATE = 0.0825\n"
      "with_tax(100)\n");

  CHECK(script.shown(1) == "42");
  CHECK(script.shown(3) == "5");
  CHECK(script.shown(7) == "28.2743338823");
  CHECK(script.shown(10) == "108.25");
}

TEST_CASE("editing above a definition recomputes through it") {
  Document document = Document::from_text(
      "k = 2\n"
      "define f(x): x * k\n"
      "f(3)\n");
  ResultCache results;
  results.refresh(document);
  CHECK(results.at(2).text == "6");

  document.replace_line(0, "k = 10");
  results.refresh(document);
  CHECK(results.at(2).text == "30");
}
