#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/engine.hpp"
#include "vim/keys.hpp"

namespace calc::test {

// Parses a keystroke script. Printable characters are literal; the keys with no
// printable form are named in angle brackets:
//
//   <esc> <cr> <bs> <del> <tab> <c-r> <c-d> <c-u>
//   <left> <right> <up> <down> <home> <end> <pgup> <pgdn>
//
// A literal '<' is written "<lt>".
std::vector<Key> parse_keys(std::string_view script);

struct Outcome {
  std::string buffer;  // newline-joined lines, no trailing newline
  Cursor cursor;
  Mode mode = Mode::Normal;
  std::string unnamed;  // contents of the unnamed register
  bool unnamed_linewise = false;
  std::string message;
  bool message_is_error = false;
  bool quit = false;
};

// Feeds `script` to a fresh engine over a buffer holding `initial`.
// One line per test case is the point: apply("1 + 2", "ggdd").buffer == "".
Outcome apply(std::string_view initial, std::string_view script);

}  // namespace calc::test
