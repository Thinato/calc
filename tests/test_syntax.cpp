#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/syntax.hpp"

using namespace calc;

namespace {

std::string described(std::string_view line) {
  std::string out;
  for (const SyntaxSpan& span : syntax_spans(line)) {
    if (!out.empty()) out += ", ";
    out += span.kind == SyntaxKind::Comment ? "comment '" : "function '";
    out += std::string(line.substr(span.begin, span.end - span.begin)) + "'";
  }
  return out;
}

}

TEST_CASE("a comment runs from its '#' to the end of the line") {
  CHECK(described("# just a note") == "comment '# just a note'");
  CHECK(described("1 + 2  # the total") == "comment '# the total'");
  CHECK(described("#") == "comment '#'");
}

TEST_CASE("a line with no comment has no comment span") {
  CHECK(described("1 + 2") == "");
  CHECK(described("") == "");
}

TEST_CASE("only the first '#' starts a comment") {
  CHECK(described("1 + 2 # a # b") == "comment '# a # b'");
}

TEST_CASE("the functions this language has are highlighted") {
  CHECK(described("sqrt(16)") == "function 'sqrt'");
  CHECK(described("pow(2, 10)") == "function 'pow'");
  CHECK(described("sqrt(16) + pow(2, 10)") == "function 'sqrt', function 'pow'");
  CHECK(described("sqrt(pow(3, 2))") == "function 'sqrt', function 'pow'");
}

TEST_CASE("a function without its parentheses is still a function") {
  CHECK(described("sqrt") == "function 'sqrt'");
  CHECK(described("sqrt + 1") == "function 'sqrt'");
}

TEST_CASE("a name that is not a function stays plain") {
  CHECK(described("foo(1)") == "");
  CHECK(described("subtotal * RATE") == "");
  CHECK(described("PI * radius ^ 2") == "");
}

TEST_CASE("a function name is matched whole, never as a prefix") {
  CHECK(described("sqrtx") == "");
  CHECK(described("xsqrt") == "");
  CHECK(described("my_pow") == "");
  CHECK(described("pow_2") == "");
  CHECK(described("sqrt2") == "");
}

TEST_CASE("a function named inside a comment is prose, not a call") {
  CHECK(described("# use sqrt(16) for that") == "comment '# use sqrt(16) for that'");
  CHECK(described("sqrt(4) # or pow(4, 0.5)") ==
        "function 'sqrt', comment '# or pow(4, 0.5)'");
}

TEST_CASE("a line that cannot be tokenized is still highlighted") {
  CHECK(described("x1 = 5  # names cannot have digits") ==
        "comment '# names cannot have digits'");
  CHECK(described("sqrt(-1 $ 2)") == "function 'sqrt'");
  CHECK(described("pow(") == "function 'pow'");
}

TEST_CASE("spans are ascending and never overlap") {
  const auto spans = syntax_spans("sqrt(pow(2, 3)) # and sqrt again");
  REQUIRE(spans.size() == 3);
  for (std::size_t index = 0; index < spans.size(); ++index) {
    CHECK(spans[index].begin < spans[index].end);
    if (index > 0) CHECK(spans[index - 1].end <= spans[index].begin);
  }
}

TEST_CASE("multi-byte characters do not disturb the spans") {
  const std::string line = "sqrt(4) # é";
  const auto spans = syntax_spans(line);
  REQUIRE(spans.size() == 2);
  CHECK(line.substr(spans[1].begin, spans[1].end - spans[1].begin) == "# é");
}
