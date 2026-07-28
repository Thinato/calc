#pragma once

#include <string>

namespace calc {

// A keystroke, normalized away from any particular terminal library so the vim
// engine can be driven from tests as easily as from FTXUI.
struct Key {
  enum class Type {
    Character,  // `text` holds one UTF-8 character
    Control,    // `text` holds the letter, e.g. "r" for Ctrl-R
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

  static Key character(std::string text) {
    return Key{Type::Character, std::move(text)};
  }
  static Key character(char value) { return Key{Type::Character, std::string(1, value)}; }
  static Key control(char letter) { return Key{Type::Control, std::string(1, letter)}; }
  static Key special(Type type) { return Key{type, {}}; }

  bool is_character(char value) const {
    return type == Type::Character && text.size() == 1 && text[0] == value;
  }
  bool is_control(char letter) const {
    return type == Type::Control && text.size() == 1 && text[0] == letter;
  }
  // The single byte this key represents, or 0 when it is not a plain ASCII
  // character. Command dispatch is byte-oriented; multi-byte input only ever
  // reaches insert mode, which uses `text` directly.
  char ascii() const {
    return type == Type::Character && text.size() == 1 ? text[0] : '\0';
  }
};

}  // namespace calc
