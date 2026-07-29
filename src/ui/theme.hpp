#pragma once

#include <ftxui/screen/color.hpp>

namespace calc::theme {

// Every colour the program uses, in one place.
inline ftxui::Color gutter() { return ftxui::Color::GrayDark; }
inline ftxui::Color gutter_current() { return ftxui::Color::GrayLight; }
inline ftxui::Color separator_dim() { return ftxui::Color::GrayDark; }
inline ftxui::Color result() { return ftxui::Color::Cyan; }
inline ftxui::Color mode_normal() { return ftxui::Color::Blue; }
inline ftxui::Color mode_insert() { return ftxui::Color::Green; }
inline ftxui::Color mode_visual() { return ftxui::Color::Magenta; }
inline ftxui::Color mode_command() { return ftxui::Color::Yellow; }
inline ftxui::Color variable() { return ftxui::Color::Yellow; }
inline ftxui::Color constant() { return ftxui::Color::Magenta; }
inline ftxui::Color error() { return ftxui::Color::Red; }
inline ftxui::Color notice() { return ftxui::Color::GrayLight; }

// The two 256-colour values here, where every other colour is a 16-colour name
// that follows the terminal's own scheme. Both want a specific shade rather than
// a role: a comment should read as dimmed text, distinctly not as dim as the
// GrayDark gutter beside it, and a function should be a pale blue that cannot be
// mistaken for the cyan result on the same line. FTXUI degrades a Palette256
// value to its nearest 16-colour index when the terminal claims no 256-colour
// support, so this costs nothing on a plainer terminal.
inline ftxui::Color comment() { return ftxui::Color::Grey62; }
inline ftxui::Color function() { return ftxui::Color::SkyBlue1; }

}  // namespace calc::theme
