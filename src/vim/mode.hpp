#pragma once

#include <string_view>

namespace calc {

enum class Mode { Normal, Insert, Visual, VisualLine, CommandLine };

inline std::string_view mode_name(Mode mode) {
  switch (mode) {
    case Mode::Normal: return "NORMAL";
    case Mode::Insert: return "INSERT";
    case Mode::Visual: return "VISUAL";
    case Mode::VisualLine: return "V-LINE";
    case Mode::CommandLine: return "COMMAND";
  }
  return "NORMAL";
}

// Where the cursor is allowed to rest. In insert mode it may sit one past the
// last character; everywhere else it sits on a character. Either way it can
// never pass the end of the typed expression, which is what keeps the result
// column unreachable.
inline bool allows_cursor_past_end(Mode mode) {
  return mode == Mode::Insert;
}

}  // namespace calc
