#include "helpers/vim_harness.hpp"

#include <map>

namespace calc::test {
namespace {

const std::map<std::string, Key>& named_keys() {
  static const std::map<std::string, Key> kKeys = {
      {"esc", Key::special(Key::Type::Escape)},
      {"cr", Key::special(Key::Type::Enter)},
      {"enter", Key::special(Key::Type::Enter)},
      {"bs", Key::special(Key::Type::Backspace)},
      {"del", Key::special(Key::Type::Delete)},
      {"tab", Key::special(Key::Type::Tab)},
      {"left", Key::special(Key::Type::Left)},
      {"right", Key::special(Key::Type::Right)},
      {"up", Key::special(Key::Type::Up)},
      {"down", Key::special(Key::Type::Down)},
      {"home", Key::special(Key::Type::Home)},
      {"end", Key::special(Key::Type::End)},
      {"pgup", Key::special(Key::Type::PageUp)},
      {"pgdn", Key::special(Key::Type::PageDown)},
      {"lt", Key::character('<')},
      {"c-r", Key::control('r')},
      {"c-d", Key::control('d')},
      {"c-u", Key::control('u')},
      {"c-e", Key::control('e')},
      {"c-y", Key::control('y')},
  };
  return kKeys;
}

}  // namespace

std::vector<Key> parse_keys(std::string_view script) {
  std::vector<Key> keys;
  std::size_t index = 0;

  while (index < script.size()) {
    if (script[index] == '<') {
      const std::size_t close = script.find('>', index);
      if (close != std::string_view::npos) {
        const std::string name(script.substr(index + 1, close - index - 1));
        const auto found = named_keys().find(name);
        if (found != named_keys().end()) {
          keys.push_back(found->second);
          index = close + 1;
          continue;
        }
      }
    }
    // A whole UTF-8 character, so multi-byte input can be scripted too.
    std::size_t length = 1;
    while (index + length < script.size() &&
           (static_cast<unsigned char>(script[index + length]) & 0xC0) == 0x80) {
      ++length;
    }
    keys.push_back(Key::character(std::string(script.substr(index, length))));
    index += length;
  }
  return keys;
}

Outcome apply(std::string_view initial, std::string_view script) {
  Document document = Document::from_text(initial);
  ResultCache results;
  results.refresh(document);
  VimEngine engine(document, results);

  for (const Key& key : parse_keys(script)) {
    engine.feed(key);
    // The renderer refreshes results once per frame; mirror that so commands
    // that read a result (gy) see current values.
    results.refresh(document);
  }

  Outcome outcome;
  outcome.buffer = document.to_text();
  if (!outcome.buffer.empty() && outcome.buffer.back() == '\n') {
    outcome.buffer.pop_back();
  }
  outcome.cursor = document.cursor();
  outcome.mode = engine.mode();
  outcome.unnamed = engine.register_value('"').text;
  outcome.unnamed_linewise = engine.register_value('"').linewise;
  outcome.message = engine.message();
  outcome.message_is_error = engine.message_is_error();
  outcome.quit = engine.quit_requested();
  return outcome;
}

}  // namespace calc::test
