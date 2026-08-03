#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/engine.hpp"
#include "vim/keys.hpp"

namespace calc::test {

std::vector<Key> parse_keys(std::string_view script);

struct Outcome {
  std::string buffer;
  Cursor cursor;
  Mode mode = Mode::Normal;
  std::string unnamed;
  bool unnamed_linewise = false;
  std::string message;
  bool message_is_error = false;
  bool quit = false;
};

Outcome apply(std::string_view initial, std::string_view script);

}
