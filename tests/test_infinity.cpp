#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/engine.hpp"
#include "doc/document.hpp"
#include "doc/results.hpp"
#include "helpers/vim_harness.hpp"
#include "vim/engine.hpp"
#include "vim/excmd.hpp"

using namespace calc;

namespace {

constexpr InfinityMode kSigned = InfinityMode::Signed;
constexpr InfinityMode kProjective = InfinityMode::Projective;

LineEval evaluated(std::string_view line, InfinityMode mode) {
  Environment environment;
  environment.set_infinity_mode(mode);
  return evaluate_line(line, environment, 0);
}

std::string shown(std::string_view line, InfinityMode mode = kSigned) {
  return evaluated(line, mode).text;
}

ErrorCode code_of(std::string_view line, InfinityMode mode = kSigned) {
  const LineEval outcome = evaluated(line, mode);
  REQUIRE(outcome.error.has_value());
  return outcome.error->code;
}

std::string message_of(std::string_view line, InfinityMode mode = kSigned) {
  const LineEval outcome = evaluated(line, mode);
  REQUIRE(outcome.error.has_value());
  return outcome.error->message;
}

class Script {
 public:
  Script(std::string_view text, InfinityMode mode)
      : document_(Document::from_text(text)) {
    results_.set_infinity_mode(mode);
    results_.refresh(document_);
  }

  std::string shown(std::size_t row) const { return results_.at(row).text; }
  std::string message(std::size_t row) const {
    REQUIRE(results_.at(row).error.has_value());
    return results_.at(row).error->message;
  }

 private:
  Document document_;
  ResultCache results_;
};

}

TEST_CASE("both spellings name one value, written the way you would type it") {
  CHECK(shown("inf") == "inf");
  CHECK(shown("infinity") == "inf");
  CHECK(shown("-inf") == "-inf");
  CHECK(shown("-infinity") == "-inf");
  CHECK(code_of("inf - infinity") == ErrorCode::DomainError);
  CHECK(code_of("infinity - inf") == ErrorCode::DomainError);
}

TEST_CASE("an infinity you typed is not restated after the line") {
  const LineEval bare = evaluated("x = inf", kSigned);
  CHECK_FALSE(bare.show_result);
  CHECK(bare.text == "inf");

  CHECK_FALSE(evaluated("x = -inf", kSigned).show_result);
  CHECK(evaluated("x = inf * 2", kSigned).show_result);
  CHECK(evaluated("x = 1 + 2", kSigned).show_result);
}

TEST_CASE("an infinite result is allowed when its inputs explain it") {
  CHECK(shown("inf + 1") == "inf");
  CHECK(shown("inf * 2") == "inf");
  CHECK(shown("inf * inf") == "inf");
  CHECK(shown("2 ^ inf") == "inf");
  CHECK(shown("inf ^ 2") == "inf");
  CHECK(shown("sqrt(inf)") == "inf");
  CHECK(shown("pow(inf, 2)") == "inf");
  CHECK(shown("1 / inf") == "0");
  CHECK(shown("-1 / inf") == "0");
}

TEST_CASE("an infinite result from finite inputs is still overflow") {
  CHECK(code_of("2 ^ 100000") == ErrorCode::NotFinite);
  CHECK(code_of("pow(10, 400)") == ErrorCode::NotFinite);
  CHECK(message_of("2 ^ 100000") == "result is too large");

  CHECK(code_of("2 ^ 100000", kProjective) == ErrorCode::NotFinite);
}

TEST_CASE("a literal too large to hold is reported where it is written") {
  CHECK(code_of("1e400") == ErrorCode::NotFinite);
  CHECK(message_of("1e400") == "number is too large");

  const LineEval outcome = evaluated("1 + 1e400", kSigned);
  REQUIRE(outcome.error.has_value());
  CHECK(outcome.error->column == 4);
}

TEST_CASE("dividing by zero answers with infinity, and keeps its sign") {
  CHECK(shown("1 / 0") == "inf");
  CHECK(shown("-1 / 0") == "-inf");
  CHECK(shown("1 / (2 - 2)") == "inf");
  CHECK(shown("0 ^ -1") == "inf");
  CHECK(shown("pow(0, -1)") == "inf");
  CHECK(shown("inf / 0") == "inf");
  CHECK(shown("-inf / 0") == "-inf");
}

TEST_CASE("zero divided by zero has no answer in either mode") {
  CHECK(code_of("0 / 0") == ErrorCode::DivisionByZero);
  CHECK(message_of("0 / 0") == "division by zero");
  CHECK(code_of("0 / 0", kProjective) == ErrorCode::DivisionByZero);
  CHECK(code_of("(2 - 2) / (2 - 2)") == ErrorCode::DivisionByZero);
}

TEST_CASE("the undefined combinations are refused in both modes") {
  for (const InfinityMode mode : {kSigned, kProjective}) {
    CHECK(code_of("inf - inf", mode) == ErrorCode::DomainError);
    CHECK(code_of("inf * 0", mode) == ErrorCode::DomainError);
    CHECK(code_of("0 * inf", mode) == ErrorCode::DomainError);
    CHECK(code_of("inf / inf", mode) == ErrorCode::DomainError);
    CHECK(message_of("inf - inf", mode) == "result is undefined");
  }
}

TEST_CASE("adding two infinities is signed arithmetic, not projective") {
  CHECK(shown("inf + inf") == "inf");
  CHECK(shown("-inf + -inf") == "-inf");
  CHECK(shown("inf - -inf") == "inf");

  CHECK(code_of("inf + inf", kProjective) == ErrorCode::DomainError);
  CHECK(code_of("inf - -inf", kProjective) == ErrorCode::DomainError);
  CHECK(message_of("inf + inf", kProjective) == "result is undefined");
}

TEST_CASE("projective mode has one point at infinity, with no sign") {
  CHECK(shown("-inf", kProjective) == "inf");
  CHECK(shown("0 - inf", kProjective) == "inf");
  CHECK(shown("-1 / 0", kProjective) == "inf");
  CHECK(shown("inf * -1", kProjective) == "inf");
  CHECK(shown("sqrt(-inf)", kProjective) == "inf");
  CHECK(shown("pow(0, -1)", kProjective) == "inf");
}

TEST_CASE("signed mode keeps the sign, so a negative infinity is out of domain") {
  CHECK(code_of("sqrt(-inf)") == ErrorCode::DomainError);
  CHECK(message_of("sqrt(-inf)") == "sqrt of a negative number");
}

TEST_CASE("the two spellings are protected the way a built-in constant is") {
  CHECK(code_of("inf = 5") == ErrorCode::ConstantReassigned);
  CHECK(message_of("inf = 5") == "inf is a built-in constant");
  CHECK(message_of("infinity = 5") == "infinity is a built-in constant");
  CHECK(message_of("define inf(x): x") == "inf is a built-in constant");
  CHECK(message_of("define infinity(x): x") == "infinity is a built-in constant");
}

TEST_CASE("a name that merely looks infinite is an ordinary constant") {
  const Script script(
      "INFINITY = 5\n"
      "INFINITY + 1\n"
      "INFINITY = 6\n",
      kSigned);

  CHECK(script.shown(1) == "6");
  CHECK(script.message(2) == "INFINITY is a constant, defined on line 1");
}

TEST_CASE("a parameter cannot be called inf, and the call says so") {
  const Script script(
      "define f(inf): inf * 2\n"
      "f(3)\n",
      kSigned);

  CHECK(script.message(1) == "in f(): inf is a built-in constant");
}

TEST_CASE("infinity crosses a function body and keeps the caller's mode") {
  const Script relaxed(
      "define rate(t): 1 / t\n"
      "rate(0)\n"
      "-rate(0)\n",
      kSigned);
  CHECK(relaxed.shown(1) == "inf");
  CHECK(relaxed.shown(2) == "-inf");

  const Script folded(
      "define rate(t): 1 / t\n"
      "rate(0)\n"
      "-rate(0)\n",
      kProjective);
  CHECK(folded.shown(1) == "inf");
  CHECK(folded.shown(2) == "inf");
}

TEST_CASE("infinity flows through a name and into the lines below") {
  const Script script(
      "limit = 1 / 0\n"
      "limit\n"
      "limit + 1\n"
      "limit - limit\n",
      kSigned);

  CHECK(script.shown(1) == "inf");
  CHECK(script.shown(2) == "inf");
  CHECK(script.message(3) == "result is undefined");
}

TEST_CASE("the mode is an ex command, and it recomputes the buffer") {
  Document document = Document::from_text("1 / 0\n-inf\ninf + inf\n");
  ResultCache results;
  VimEngine engine(document, results);

  const auto settle = [&] {
    results.set_infinity_mode(engine.infinity_mode());
    results.refresh(document);
  };

  settle();
  CHECK(results.at(0).text == "inf");
  CHECK(results.at(1).text == "-inf");
  CHECK(results.at(2).text == "inf");

  for (const Key& key : test::parse_keys(":set infinity=projective<cr>")) {
    engine.feed(key);
  }
  settle();
  CHECK(results.at(0).text == "inf");
  CHECK(results.at(1).text == "inf");
  REQUIRE(results.at(2).error.has_value());
  CHECK(results.at(2).error->message == "result is undefined");

  for (const Key& key : test::parse_keys(":set infinity=signed<cr>")) {
    engine.feed(key);
  }
  settle();
  CHECK(results.at(1).text == "-inf");
}

TEST_CASE("the mode reports itself and refuses what it does not know") {
  Document document;
  ResultCache results;
  results.refresh(document);

  CHECK(execute_ex_command("set infinity?", document, &results).message ==
        "infinity=signed");
  CHECK(execute_ex_command("set infinity", document, &results).message ==
        "infinity=signed");

  const ExOutcome chosen = execute_ex_command("set infinity=projective", document);
  REQUIRE(chosen.infinity_mode.has_value());
  CHECK(*chosen.infinity_mode == InfinityMode::Projective);
  CHECK_FALSE(chosen.is_error);

  const ExOutcome refused = execute_ex_command("set infinity=nonsense", document);
  CHECK(refused.is_error);
  CHECK(refused.message == "unknown mode: nonsense (try signed or projective)");
  CHECK_FALSE(refused.infinity_mode.has_value());

  CHECK(execute_ex_command("set wrapscan", document).is_error);
}

TEST_CASE("a plot needs finite corners, so inf is not a range") {
  Document document = Document::from_text("define f(x): x\n");
  ResultCache results;
  results.refresh(document);

  CHECK(execute_ex_command("plot f 0..inf", document, &results).message ==
        "bad range: 0..inf");
}
