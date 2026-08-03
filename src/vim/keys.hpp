#pragma once

#include <string>

namespace calc {

struct Key {
  enum class Type {
    Character,
    Control,
    Escape,
    Enter,
    Backspace,
    Delete,
    Tab,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Unknown,
  };

  Type type = Type::Unknown;
  std::string text;

  static Key character(std::string text) { return Key{Type::Character, std::move(text)}; }
  static Key character(char value) { return Key{Type::Character, std::string(1, value)}; }
  static Key control(char letter) { return Key{Type::Control, std::string(1, letter)}; }
  static Key special(Type type) { return Key{type, {}}; }

  bool is_character(char value) const {
    return type == Type::Character && text.size() == 1 && text[0] == value;
  }
  bool is_control(char letter) const {
    return type == Type::Control && text.size() == 1 && text[0] == letter;
  }
  char ascii() const {
    return type == Type::Character && text.size() == 1 ? text[0] : '\0';
  }
};

}
