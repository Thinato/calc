#include "ui/app.hpp"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "doc/results.hpp"
#include "ui/layout.hpp"
#include "vim/engine.hpp"
#include "vim/keys.hpp"

namespace calc {
namespace {

// Translates a terminal event into the engine's own key type. Specials are
// matched before the control letters because several overlap: Tab is Ctrl-I and
// Return is Ctrl-M at the byte level.
std::optional<Key> to_key(const ftxui::Event& event) {
  using ftxui::Event;

  if (event == Event::Escape) return Key::special(Key::Type::Escape);
  if (event == Event::Return) return Key::special(Key::Type::Enter);
  if (event == Event::Backspace) return Key::special(Key::Type::Backspace);
  if (event == Event::Delete) return Key::special(Key::Type::Delete);
  if (event == Event::Tab) return Key::special(Key::Type::Tab);
  if (event == Event::ArrowLeft) return Key::special(Key::Type::Left);
  if (event == Event::ArrowRight) return Key::special(Key::Type::Right);
  if (event == Event::ArrowUp) return Key::special(Key::Type::Up);
  if (event == Event::ArrowDown) return Key::special(Key::Type::Down);
  if (event == Event::Home) return Key::special(Key::Type::Home);
  if (event == Event::End) return Key::special(Key::Type::End);
  if (event == Event::PageUp) return Key::special(Key::Type::PageUp);
  if (event == Event::PageDown) return Key::special(Key::Type::PageDown);

  if (event == Event::CtrlR) return Key::control('r');
  if (event == Event::CtrlD) return Key::control('d');
  if (event == Event::CtrlU) return Key::control('u');
  if (event == Event::CtrlE) return Key::control('e');
  if (event == Event::CtrlY) return Key::control('y');

  if (event.is_character()) return Key::character(event.character());
  return std::nullopt;
}

void copy_to_clipboard(const std::string& text) {
#if defined(__APPLE__)
  const char* command = "pbcopy";
#elif defined(__linux__)
  const char* command = "xclip -selection clipboard";
#else
  const char* command = nullptr;
#endif
  if (command == nullptr) return;

  // NOLINTNEXTLINE(cert-env33-c) - a fixed command with no interpolated input
  FILE* pipe = popen(command, "w");
  if (pipe == nullptr) return;
  std::fwrite(text.data(), 1, text.size(), pipe);
  pclose(pipe);
}

}  // namespace

int run_editor(Document& document) {
  ResultCache results;
  VimEngine engine(document, results);
  engine.set_clipboard_writer(&copy_to_clipboard);
  Viewport viewport;

  auto screen = ftxui::ScreenInteractive::Fullscreen();

  auto view = ftxui::Renderer([&] {
    const auto rows = static_cast<std::size_t>(std::max(screen.dimy(), 1));
    viewport.height = rows > kChromeRows ? rows - kChromeRows : 1;

    apply_scroll(viewport, engine.take_scroll_request(), document);
    follow_cursor(viewport, document);
    results.refresh(document);
    return render_frame(document, results, engine, viewport);
  });

  view |= ftxui::CatchEvent([&](const ftxui::Event& event) {
    const std::optional<Key> key = to_key(event);
    if (!key) return false;
    engine.feed(*key);
    if (engine.quit_requested()) screen.Exit();
    return true;
  });

  screen.Loop(view);
  return 0;
}

}  // namespace calc
