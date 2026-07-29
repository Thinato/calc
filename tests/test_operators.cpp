#include <doctest/doctest.h>

#include "helpers/vim_harness.hpp"

using namespace calc;
using calc::test::apply;

namespace {

std::string buffer_after(std::string_view initial, std::string_view script) {
  return apply(initial, script).buffer;
}

}  // namespace

TEST_CASE("delete composes with every motion") {
  CHECK(buffer_after("1 + 2", "dl") == " + 2");
  CHECK(buffer_after("1 + 2", "d$") == "");
  CHECK(buffer_after("1 + 2", "$dh") == "1 +2");  // h is exclusive of the cursor
  CHECK(buffer_after("1 + 2", "dw") == "+ 2");
  CHECK(buffer_after("1 + 2", "$d0") == "2");
  CHECK(buffer_after("sqrt(16)", "df(") == "16)");
  CHECK(buffer_after("sqrt(16)", "dt(") == "(16)");
  CHECK(buffer_after("(1 + 2) * 3", "d%") == " * 3");
}

TEST_CASE("a doubled operator key works on whole lines") {
  CHECK(buffer_after("a\nb\nc", "dd") == "b\nc");
  CHECK(buffer_after("a\nb\nc", "jdd") == "a\nc");
  CHECK(buffer_after("a\nb\nc", "2dd") == "c");
  CHECK(buffer_after("a\nb\nc\nd", "j2dd") == "a\nd");
  CHECK(buffer_after("a", "dd") == "");  // one empty line always remains
}

TEST_CASE("counts multiply between the operator and the motion") {
  CHECK(buffer_after("a b c d e f g", "d3w") == "d e f g");
  CHECK(buffer_after("a b c d e f g", "2d3w") == "g");
}

TEST_CASE("dw on the last word of a line does not pull the next line up") {
  CHECK(buffer_after("one two\nthree", "wdw") == "one \nthree");
}

TEST_CASE("D and C act to the end of the line") {
  CHECK(buffer_after("1 + 2", "llD") == "1 ");
  const auto changed = apply("1 + 2", "llC");
  CHECK(changed.buffer == "1 ");
  CHECK(changed.mode == Mode::Insert);
}

TEST_CASE("x and X delete single characters") {
  CHECK(buffer_after("1 + 2", "x") == " + 2");
  CHECK(buffer_after("1 + 2", "3x") == " 2");
  CHECK(buffer_after("1 + 2", "$X") == "1 +2");
  CHECK(buffer_after("", "x") == "");            // nothing to delete
  CHECK(buffer_after("1 + 2", "X") == "1 + 2");  // nothing to the left
}

TEST_CASE("change enters insert mode with the text removed") {
  const auto outcome = apply("1 + 2", "cw");
  CHECK(outcome.buffer == " + 2");
  CHECK(outcome.mode == Mode::Insert);
}

TEST_CASE("cw on a word behaves like ce, leaving the trailing space") {
  CHECK(buffer_after("one two", "cwX<esc>") == "X two");
}

TEST_CASE("cc blanks the line but keeps it and its indent") {
  const auto outcome = apply("  1 + 2\nnext", "cc");
  CHECK(outcome.buffer == "  \nnext");
  CHECK(outcome.mode == Mode::Insert);
  CHECK(outcome.cursor.column == 2);
}

TEST_CASE("cc on the only line does not remove it") {
  CHECK(buffer_after("abc", "ccX<esc>") == "X");
}

TEST_CASE("S changes a whole line, Y yanks one") {
  CHECK(buffer_after("abc\ndef", "SX<esc>") == "X\ndef");
  const auto yanked = apply("abc\ndef", "Y");
  CHECK(yanked.unnamed == "abc\n");
  CHECK(yanked.unnamed_linewise);
}

TEST_CASE("yank fills the unnamed register without changing the buffer") {
  const auto charwise = apply("1 + 2", "yw");
  CHECK(charwise.buffer == "1 + 2");
  CHECK(charwise.unnamed == "1 ");
  CHECK_FALSE(charwise.unnamed_linewise);

  const auto linewise = apply("a\nb", "yy");
  CHECK(linewise.unnamed == "a\n");
  CHECK(linewise.unnamed_linewise);
}

TEST_CASE("delete also fills the unnamed register") {
  CHECK(apply("1 + 2", "dw").unnamed == "1 ");
  CHECK(apply("a\nb", "dd").unnamed == "a\n");
}

TEST_CASE("named registers keep separate contents") {
  // Yank a line into "a, delete a different one, then put "a back.
  CHECK(buffer_after("keep\ndrop", "\"ayyjdd\"ap") == "keep\nkeep");
}

TEST_CASE("put inserts linewise text on its own line") {
  CHECK(buffer_after("a\nb", "yyp") == "a\na\nb");
  CHECK(buffer_after("a\nb", "yyP") == "a\na\nb");
  CHECK(buffer_after("a\nb", "yyjp") == "a\nb\na");
  CHECK(buffer_after("a\nb", "yy2p") == "a\na\na\nb");
}

TEST_CASE("put inserts charwise text inline") {
  CHECK(buffer_after("ab", "ylp") == "aab");
  CHECK(buffer_after("ab", "ylP") == "aab");
  CHECK(buffer_after("ab", "yl3p") == "aaaab");
}

TEST_CASE("charwise put spanning lines splits the line") {
  // The selection covers "ab\nc", so putting it after 'b' splits line 0.
  CHECK(buffer_after("ab\ncd", "vjy$p") == "abab\nc\ncd");
}

TEST_CASE("r replaces characters in place") {
  CHECK(buffer_after("1 + 2", "rX") == "X + 2");
  CHECK(buffer_after("1 + 2", "3rX") == "XXX 2");
  // A count longer than the line leaves it untouched, as vim does.
  CHECK(buffer_after("ab", "5rX") == "ab");
}

TEST_CASE("tilde toggles case and moves on") {
  CHECK(buffer_after("abc", "~") == "Abc");
  CHECK(buffer_after("abc", "3~") == "ABC");
  CHECK(buffer_after("AbC", "3~") == "aBc");
}

TEST_CASE("J joins lines with a single space") {
  CHECK(buffer_after("1 +\n2", "J") == "1 + 2");
  CHECK(buffer_after("a\nb\nc", "3J") == "a b c");
  CHECK(buffer_after("a \nb", "J") == "a b");    // no doubled space
  CHECK(buffer_after("a\n   b", "J") == "a b");  // leading blanks dropped
}

TEST_CASE("undo and redo step through whole commands") {
  CHECK(buffer_after("a\nb\nc", "ddu") == "a\nb\nc");
  CHECK(buffer_after("a\nb\nc", "dddduu") == "a\nb\nc");
  CHECK(buffer_after("a\nb\nc", "ddu<c-r>") == "b\nc");
}

TEST_CASE("an insert session undoes as one step") {
  CHECK(buffer_after("", "iabc<esc>u") == "");
  CHECK(buffer_after("x", "ohello<esc>u") == "x");
}

TEST_CASE("dot repeats the last change") {
  CHECK(buffer_after("a\nb\nc\nd", "dd.") == "c\nd");
  CHECK(buffer_after("1 + 2", "x.") == "+ 2");
  // `r` leaves the cursor on the character it replaced, so `.` hits it again.
  CHECK(buffer_after("aaa", "rX.") == "Xaa");
  CHECK(buffer_after("aaa", "rXl.") == "XXa");
}

TEST_CASE("dot repeats a whole insert session") {
  CHECK(buffer_after("", "ihi<esc>.") == "hhii");
}

TEST_CASE("dot does not repeat an undo") {
  // `u` changes the buffer but is not a change, so `.` must ignore it.
  CHECK(buffer_after("a\nb\nc", "ddu.") == "b\nc");
}

TEST_CASE("insert mode edits") {
  CHECK(buffer_after("", "i1 + 2<esc>") == "1 + 2");
  CHECK(buffer_after("ac", "lib<esc>") == "abc");
  CHECK(buffer_after("ab", "ax<esc>") == "axb");
  CHECK(buffer_after("ab", "Ax<esc>") == "abx");
  CHECK(buffer_after("  ab", "Ix<esc>") == "  xab");
  CHECK(buffer_after("a", "ob<esc>") == "a\nb");
  CHECK(buffer_after("a", "Ob<esc>") == "b\na");
}

TEST_CASE("backspace and enter in insert mode") {
  CHECK(buffer_after("", "iabc<bs><esc>") == "ab");
  CHECK(buffer_after("", "iab<cr>cd<esc>") == "ab\ncd");
  // Backspace at the start of a line joins it to the one above.
  CHECK(buffer_after("ab\ncd", "ji<bs><esc>") == "abcd");
}

TEST_CASE("escape from insert steps the cursor back one character") {
  const auto outcome = apply("", "iabc<esc>");
  CHECK(outcome.cursor.column == 2);  // on 'c', not past it
  CHECK(outcome.mode == Mode::Normal);
}

TEST_CASE("visual mode selects and operates") {
  CHECK(buffer_after("1 + 2", "vlld") == " 2");
  CHECK(buffer_after("1 + 2", "vd") == " + 2");  // one character
  CHECK(buffer_after("1 + 2", "v$d") == "");
  CHECK(buffer_after("a\nb\nc", "Vjd") == "c");
  CHECK(buffer_after("a\nb\nc", "Vd") == "b\nc");

  const auto yanked = apply("1 + 2", "vlly");
  CHECK(yanked.buffer == "1 + 2");
  CHECK(yanked.unnamed == "1 +");
  CHECK(yanked.mode == Mode::Normal);
}

TEST_CASE("visual mode leaves on escape and toggles off on its own key") {
  CHECK(apply("abc", "v").mode == Mode::Visual);
  CHECK(apply("abc", "v<esc>").mode == Mode::Normal);
  CHECK(apply("abc", "vv").mode == Mode::Normal);
  CHECK(apply("abc", "V").mode == Mode::VisualLine);
  CHECK(apply("abc", "VV").mode == Mode::Normal);
}

TEST_CASE("visual o swaps which end the cursor controls") {
  const auto outcome = apply("abcdef", "vllo");
  CHECK(outcome.cursor.column == 0);
}

TEST_CASE("visual change enters insert mode") {
  const auto outcome = apply("1 + 2", "vlc");
  CHECK(outcome.buffer == "+ 2");
  CHECK(outcome.mode == Mode::Insert);
}

TEST_CASE("search moves the cursor and n repeats it") {
  const auto found = apply("1 + 2\nsqrt(9)\n3 * 4", "/sqrt<cr>");
  CHECK(found.cursor.row == 1);
  CHECK(found.cursor.column == 0);

  // Two matches, forward then wrap back to the first.
  const auto wrapped = apply("aXa\nbXb", "/X<cr>n");
  CHECK(wrapped.cursor.row == 1);
  CHECK(apply("aXa\nbXb", "/X<cr>nn").cursor.row == 0);
}

TEST_CASE("a failed search reports itself and stays put") {
  const auto outcome = apply("1 + 2", "/zzz<cr>");
  CHECK(outcome.cursor.column == 0);
  CHECK(outcome.message_is_error);
}

TEST_CASE("backward search") {
  const auto outcome = apply("aXa\nbbb\ncXc", "G?X<cr>");
  CHECK(outcome.cursor.row == 0);
}

TEST_CASE("escape abandons a command line without running it") {
  const auto outcome = apply("a\nb", ":q<esc>");
  CHECK_FALSE(outcome.quit);
  CHECK(outcome.mode == Mode::Normal);
}

TEST_CASE("gy yanks the result of the current line") {
  const auto outcome = apply("1 + 2\nsqrt(16)", "gy");
  CHECK(outcome.unnamed == "3");
  CHECK(outcome.buffer == "1 + 2\nsqrt(16)");  // nothing changed

  CHECK(apply("1 + 2\nsqrt(16)", "jgy").unnamed == "4");
  CHECK(apply("1 + 2", "gY").unnamed == "1 + 2 = 3");
}

TEST_CASE("gy reaches the value of a definition even when it is not shown") {
  CHECK(apply("x = 5", "gy").unnamed == "5");
  // gY would otherwise produce "x = 5 = 5".
  CHECK(apply("x = 5", "gY").unnamed == "x = 5");
  CHECK(apply("x = 1 + 2", "gY").unnamed == "x = 1 + 2 = 3");
}

TEST_CASE("gy on a line with no result says so") {
  const auto outcome = apply("not an expression", "gy");
  CHECK(outcome.message_is_error);
  CHECK(outcome.unnamed.empty());
}
