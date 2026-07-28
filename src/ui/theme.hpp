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
inline ftxui::Color error() { return ftxui::Color::Red; }
inline ftxui::Color notice() { return ftxui::Color::GrayLight; }
inline ftxui::Color comment() { return ftxui::Color::GrayDark; }

}  // namespace calc::theme
