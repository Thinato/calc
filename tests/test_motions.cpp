#include <doctest/doctest.h>

#include "helpers/vim_harness.hpp"

using namespace calc;
using calc::test::apply;

namespace {

std::size_t column_after(std::string_view line, std::string_view script) {
  return apply(line, script).cursor.column;
}

}

TEST_CASE("hjkl move by one, and stop at the edges") {
  CHECK(column_after("1 + 2", "ll") == 2);
  CHECK(column_after("1 + 2", "llh") == 1);
  CHECK(column_after("1 + 2", "hhhh") == 0);
  CHECK(column_after("1 + 2", "llllllllll") == 4);

  const auto down = apply("a\nb\nc", "jj");
  CHECK(down.cursor.row == 2);
  CHECK(apply("a\nb\nc", "jjjjj").cursor.row == 2);
  CHECK(apply("a\nb\nc", "jjk").cursor.row == 1);
  CHECK(apply("a\nb\nc", "kkk").cursor.row == 0);
}

TEST_CASE("arrow keys mirror hjkl") {
  CHECK(column_after("1 + 2", "<right><right>") == 2);
  CHECK(apply("a\nb", "<down>").cursor.row == 1);
  CHECK(column_after("1 + 2", "<end>") == 4);
  CHECK(column_after("1 + 2", "<end><home>") == 0);
}

TEST_CASE("a count repeats a motion") {
  CHECK(column_after("0123456789", "5l") == 5);
  CHECK(column_after("0123456789", "12l") == 9);
  CHECK(apply("a\nb\nc\nd\ne", "3j").cursor.row == 3);
}

TEST_CASE("line motions") {
  CHECK(column_after("  1 + 2", "$") == 6);
  CHECK(column_after("  1 + 2", "$0") == 0);
  CHECK(column_after("  1 + 2", "$^") == 2);
  CHECK(column_after("1 + 2", "3|") == 2);
}

TEST_CASE("$ stops on the last character, never past it") {
  CHECK(column_after("1 + 2", "$") == 4);
  CHECK(apply("1 + 2", "$").mode == Mode::Normal);
}

TEST_CASE("gg and G jump to the first and last lines") {
  CHECK(apply("a\nb\nc\nd", "G").cursor.row == 3);
  CHECK(apply("a\nb\nc\nd", "Ggg").cursor.row == 0);
  CHECK(apply("a\nb\nc\nd", "3G").cursor.row == 2);
  CHECK(apply("a\nb\nc\nd", "2gg").cursor.row == 1);
  CHECK(apply("  indented\nb", "G^").cursor.column == 0);
  CHECK(apply("b\n  indented", "G").cursor.column == 2);
}

TEST_CASE("word motions treat operators as their own words") {
  CHECK(column_after("1+2", "w") == 1);
  CHECK(column_after("1+2", "ww") == 2);
  CHECK(column_after("sqrt(16)", "w") == 4);
  CHECK(column_after("sqrt(16)", "ww") == 5);
  CHECK(column_after("1 + 2", "w") == 2);
  CHECK(column_after("1 + 2", "www") == 4);
}

TEST_CASE("W treats punctuation as part of the word") {
  CHECK(column_after("1+2 3+4", "W") == 4);
}

TEST_CASE("b walks back to the start of a word") {
  CHECK(column_after("1 + 2", "$b") == 2);
  CHECK(column_after("1 + 2", "$bb") == 0);
  CHECK(column_after("sqrt(16)", "$b") == 5);
  CHECK(column_after("abc def", "$b") == 4);
}

TEST_CASE("e lands on the last character of a word") {
  CHECK(column_after("abc def", "e") == 2);
  CHECK(column_after("abc def", "ee") == 6);
  CHECK(column_after("sqrt(16)", "e") == 3);
}

TEST_CASE("word motions cross line boundaries") {
  const auto outcome = apply("abc\ndef", "w");
  CHECK(outcome.cursor.row == 1);
  CHECK(outcome.cursor.column == 0);

  const auto backwards = apply("abc\ndef", "jb");
  CHECK(backwards.cursor.row == 0);
  CHECK(backwards.cursor.column == 0);
}

TEST_CASE("f and t search within the line") {
  CHECK(column_after("pow(2, 10)", "f,") == 5);
  CHECK(column_after("pow(2, 10)", "t,") == 4);
  CHECK(column_after("a.b.c", "2f.") == 3);
  CHECK(column_after("pow(2, 10)", "$F,") == 5);
  CHECK(column_after("pow(2, 10)", "$T,") == 6);
  CHECK(column_after("1 + 2", "fz") == 0);
}

TEST_CASE("percent jumps between matching parentheses") {
  CHECK(column_after("(1 + 2)", "%") == 6);
  CHECK(column_after("(1 + 2)", "$%") == 0);
  CHECK(column_after("((1))", "%") == 4);
  CHECK(column_after("sqrt(16)", "%") == 7);
}

TEST_CASE("percent matches parentheses split across lines") {
  const auto outcome = apply("(1 +\n2)", "%");
  CHECK(outcome.cursor.row == 1);
  CHECK(outcome.cursor.column == 1);
}

TEST_CASE("braces move by paragraph") {
  CHECK(apply("a\nb\n\nc\nd", "}").cursor.row == 2);
  CHECK(apply("a\nb\n\nc\nd", "G{").cursor.row == 2);
}

TEST_CASE("j and k remember the column they started from") {
  const auto outcome = apply("abcdef\nx\nabcdef", "5ljj");
  CHECK(outcome.cursor.row == 2);
  CHECK(outcome.cursor.column == 5);
}

TEST_CASE("$ then j sticks to the end of each line") {
  const auto outcome = apply("abcdef\nxy", "$j");
  CHECK(outcome.cursor.row == 1);
  CHECK(outcome.cursor.column == 1);
}
