#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "core/engine.hpp"
#include "core/environment.hpp"
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
  const Environment& environment() const { return results_.environment(); }

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

ErrorCode single_line_code(std::string_view line) {
  const LineEval outcome = evaluate_line(line);
  REQUIRE(outcome.error.has_value());
  return outcome.error->code;
}

}

TEST_CASE("valid names are accepted") {
  for (std::string_view name : {"x", "y", "test", "helloWorld", "xOne", "xTwo", "_x",
                                "under_score", "TEST", "TEST_ONE", "PI"}) {
    CAPTURE(name);
    const std::string line = std::string(name) + " = 2";
    const LineEval outcome = evaluate_line(line);
    if (name == "PI") {
      CHECK(outcome.error.has_value());
    } else {
      CHECK_FALSE(outcome.error.has_value());
      CHECK(outcome.assigned_name == std::string(name));
    }
  }
}

TEST_CASE("names cannot contain digits") {
  CHECK(single_line_code("x1") == ErrorCode::InvalidName);
  CHECK(single_line_code("x2 = 5") == ErrorCode::InvalidName);
  CHECK(single_line_code("1 + x1") == ErrorCode::InvalidName);
  CHECK(single_line_code("abc123") == ErrorCode::InvalidName);
}

TEST_CASE("names cannot start with a digit") {
  CHECK(single_line_code("1_x") == ErrorCode::InvalidName);
  CHECK(single_line_code("1_x = 5") == ErrorCode::InvalidName);
  CHECK(single_line_code("1_000") == ErrorCode::InvalidName);
  CHECK(single_line_code("2PI") == ErrorCode::InvalidName);
}

TEST_CASE("names cannot contain a dot") {
  CHECK(single_line_code("x.y") == ErrorCode::InvalidName);
  CHECK(single_line_code("x.y = 5") == ErrorCode::InvalidName);
  CHECK(single_line_code("x.5") == ErrorCode::InvalidName);
}

TEST_CASE("a malformed name is quoted whole, with its column") {
  const LineEval outcome = evaluate_line("1 + x1 * 2");
  REQUIRE(outcome.error.has_value());
  CHECK(outcome.error->message.find("'x1'") != std::string::npos);
  CHECK(outcome.error->message.find("digits") != std::string::npos);
  CHECK(outcome.error->column == 4);
}

TEST_CASE("valid numbers still lex") {
  CHECK(evaluate_line("1e5").text == "100000");
  CHECK(evaluate_line("1.5e-3").text == "0.0015");
  CHECK(evaluate_line(".5 + .5").text == "1");
  CHECK(evaluate_line("128.40").text == "128.4");
}

TEST_CASE("is_constant_name splits the two kinds by spelling alone") {
  CHECK(Environment::is_constant_name("PI"));
  CHECK(Environment::is_constant_name("E"));
  CHECK(Environment::is_constant_name("TAU"));
  CHECK(Environment::is_constant_name("TEST"));
  CHECK(Environment::is_constant_name("TEST_ONE"));
  CHECK(Environment::is_constant_name("_"));

  CHECK_FALSE(Environment::is_constant_name("x"));
  CHECK_FALSE(Environment::is_constant_name("test"));
  CHECK_FALSE(Environment::is_constant_name("helloWorld"));
  CHECK_FALSE(Environment::is_constant_name("xOne"));
  CHECK_FALSE(Environment::is_constant_name("TESTa"));
}

TEST_CASE("the left of '=' must be a bare name") {
  CHECK(single_line_code("(x) = 5") == ErrorCode::AssignmentTarget);
  CHECK(single_line_code("1 = 2") == ErrorCode::AssignmentTarget);
  CHECK(single_line_code("x + 1 = 5") == ErrorCode::AssignmentTarget);
  CHECK(single_line_code("sqrt(4) = 5") == ErrorCode::AssignmentTarget);
  CHECK(single_line_code("= 5") == ErrorCode::AssignmentTarget);
}

TEST_CASE("assignment does not chain") {
  CHECK(single_line_code("x = y = 5") == ErrorCode::MultipleAssignment);
}

TEST_CASE("a name cannot shadow a function") {
  CHECK(single_line_code("sqrt = 5") == ErrorCode::NameIsFunction);
  CHECK(single_line_code("pow = 5") == ErrorCode::NameIsFunction);
}

TEST_CASE("an incomplete assignment reports the missing value") {
  CHECK(single_line_code("x =") == ErrorCode::UnexpectedEnd);
}

TEST_CASE("an unknown function is still distinguished from a name") {
  CHECK(single_line_code("sin(1)") == ErrorCode::UnknownFunction);
  CHECK(single_line_code("sqrt 16") == ErrorCode::ExpectedCallParen);
  CHECK(single_line_code("sin") == ErrorCode::UndefinedName);
}

TEST_CASE("PI, E and TAU are built in") {
  CHECK(evaluate_line("PI").text == "3.14159265359");
  CHECK(evaluate_line("E").text == "2.71828182846");
  CHECK(evaluate_line("TAU").text == "6.28318530718");
  CHECK(evaluate_line("2 * PI").text == evaluate_line("TAU").text);
  CHECK(evaluate_line("PI * 2 ^ 2").text == "12.5663706144");
}

TEST_CASE("built-in constants cannot be reassigned") {
  for (std::string_view name : {"PI", "E", "TAU"}) {
    CAPTURE(name);
    const std::string line = std::string(name) + " = 3";
    const LineEval outcome = evaluate_line(line);
    REQUIRE(outcome.error.has_value());
    CHECK(outcome.error->code == ErrorCode::ConstantReassigned);
    CHECK(outcome.error->message.find("built-in") != std::string::npos);
  }
}

TEST_CASE("a name is usable on the lines below its definition") {
  const Script script("x = 5\nx * 3\nx + x");
  CHECK_FALSE(script.failed(0));
  CHECK(script.shown(1) == "15");
  CHECK(script.shown(2) == "10");
}

TEST_CASE("a name is not usable above its definition") {
  const Script script("x * 2\nx = 5");
  CHECK(script.code(0) == ErrorCode::UndefinedName);
  CHECK(script.message(0).find("'x'") != std::string::npos);
  CHECK_FALSE(script.failed(1));
}

TEST_CASE("a variable reassigns, and the lines below see the new value") {
  const Script script("x = 2\nx * 10\nx = 7\nx * 10");
  CHECK(script.shown(1) == "20");
  CHECK(script.shown(3) == "70");
}

TEST_CASE("x = x + 1 reads as an increment") {
  const Script script("x = 5\nx = x + 1\nx");
  CHECK_FALSE(script.failed(1));
  CHECK(script.shown(1) == "6");
  CHECK(script.shown(2) == "6");
}

TEST_CASE("a self-reference with nothing to build on is undefined") {
  const Script script("x = x + 1");
  CHECK(script.code(0) == ErrorCode::UndefinedName);
}

TEST_CASE("names compose with functions and each other") {
  const Script script("side = 3\narea = side ^ 2\nsqrt(area)\nRATE = 0.5\narea * RATE");
  CHECK(script.shown(1) == "9");
  CHECK(script.shown(2) == "3");
  CHECK(script.shown(4) == "4.5");
}

TEST_CASE("a user constant cannot be reassigned") {
  const Script script("TEST = 2\nTEST * 2\nTEST = 5\nTEST * 2");
  CHECK_FALSE(script.failed(0));
  CHECK(script.shown(1) == "4");

  CHECK(script.code(2) == ErrorCode::ConstantReassigned);
  CHECK(script.message(2).find("line 1") != std::string::npos);
  CHECK(script.message(2).find("TEST") != std::string::npos);

  CHECK(script.shown(3) == "4");
}

TEST_CASE("underscored constant names work") {
  const Script script("TEST_ONE = 2\nTEST_TWO = 3\nTEST_ONE * TEST_TWO\nTEST_ONE = 9");
  CHECK(script.shown(2) == "6");
  CHECK(script.code(3) == ErrorCode::ConstantReassigned);
}

TEST_CASE("the reassignment error names the right line") {
  const Script script("\n\nRATE = 0.1\n\n\nRATE = 0.2");
  CHECK(script.message(5).find("line 3") != std::string::npos);
}

TEST_CASE("a constant and a variable of similar spelling are different names") {
  const Script script("TEST = 2\ntest = 3\nTEST + test\ntest = 4\ntest");
  CHECK(script.shown(2) == "5");
  CHECK_FALSE(script.failed(3));
  CHECK(script.shown(4) == "4");
}

TEST_CASE("the environment records which names are constants") {
  const Script script("x = 1\nTEST = 2");
  const Binding* variable = script.environment().find("x");
  const Binding* constant = script.environment().find("TEST");
  REQUIRE(variable != nullptr);
  REQUIRE(constant != nullptr);
  CHECK_FALSE(variable->is_constant);
  CHECK(constant->is_constant);
  CHECK(constant->defined_row == 1);
}

TEST_CASE("a definition shows its result when the value was computed") {
  const Script script("x = 1 + 2");
  CHECK(script.shown(0) == "3");
  CHECK(script.line(0).show_result);
  CHECK(script.line(0).assigned_name == "x");
  CHECK_FALSE(script.line(0).assigned_constant);
}

TEST_CASE("a definition stays quiet when the value was typed literally") {
  for (std::string_view line :
       {"x = 5", "x = 128.40", "x = -5", "x = 1e3", "x = (5)", "x = -(5)"}) {
    CAPTURE(line);
    const Script script(line);
    CHECK_FALSE(script.line(0).show_result);
    CHECK(script.line(0).has_result());
  }
}

TEST_CASE("a computed value is shown") {
  for (std::string_view line :
       {"x = 1 + 1", "x = sqrt(4)", "x = 2 * 3", "x = PI", "x = -(1 + 1)"}) {
    CAPTURE(line);
    const Script script(line);
    CHECK(script.line(0).show_result);
  }
}

TEST_CASE("a bare expression is not an assignment") {
  const Script script("1 + 2");
  CHECK_FALSE(script.line(0).is_assignment());
  CHECK(script.line(0).show_result);
}

TEST_CASE("the assignment target is located for highlighting") {
  const Script script("  total = 1 + 2");
  REQUIRE(script.line(0).is_assignment());
  CHECK(script.line(0).assigned_column == 2);
  CHECK(script.line(0).assigned_name == "total");
}

TEST_CASE("editing a definition recomputes every line below it") {
  Document document = Document::from_text("x = 2\nx * 10\nx + 1");
  ResultCache results;
  results.refresh(document);
  CHECK(results.at(1).text == "20");
  CHECK(results.at(2).text == "3");

  document.replace_line(0, "x = 5");
  results.refresh(document);
  CHECK(results.at(1).text == "50");
  CHECK(results.at(2).text == "6");
}

TEST_CASE("deleting a definition makes the lines below it undefined") {
  Document document = Document::from_text("x = 2\nx * 10");
  ResultCache results;
  results.refresh(document);
  REQUIRE(results.at(1).text == "20");

  document.erase_lines(0, 1);
  results.refresh(document);
  CHECK_FALSE(results.at(0).has_result());
  REQUIRE(results.at(0).error.has_value());
  CHECK(results.at(0).error->code == ErrorCode::UndefinedName);
}

TEST_CASE("inserting a definition above a use resolves it") {
  Document document = Document::from_text("x * 3");
  ResultCache results;
  results.refresh(document);
  REQUIRE(results.at(0).error.has_value());

  document.insert_lines(0, {"x = 4"});
  results.refresh(document);
  CHECK(results.at(1).text == "12");
}

TEST_CASE("moving a definition below its use breaks it again") {
  Document document = Document::from_text("x = 4\nx * 3");
  ResultCache results;
  results.refresh(document);
  REQUIRE(results.at(1).text == "12");

  document.replace_line(0, "x * 3");
  document.replace_line(1, "x = 4");
  results.refresh(document);
  CHECK(results.at(0).error->code == ErrorCode::UndefinedName);
}

TEST_CASE("comments and blank lines do not disturb the environment") {
  const Script script("x = 2\n\n# a note\nx * 3");
  CHECK(script.shown(3) == "6");
  CHECK_FALSE(script.line(1).has_result());
  CHECK_FALSE(script.line(2).has_result());
}

TEST_CASE("the worked example from the brief") {
  const Script script(
      "subtotal = 128.40\n"
      "RATE = 0.0825\n"
      "tip = subtotal * 0.2\n"
      "subtotal * RATE\n"
      "subtotal + tip");

  CHECK_FALSE(script.line(0).show_result);
  CHECK_FALSE(script.line(1).show_result);
  CHECK(script.shown(2) == "25.68");
  CHECK(script.shown(3) == "10.593");
  CHECK(script.shown(4) == "154.08");
}
