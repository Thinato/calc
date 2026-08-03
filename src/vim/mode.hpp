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

inline bool allows_cursor_past_end(Mode mode) { return mode == Mode::Insert; }

}
