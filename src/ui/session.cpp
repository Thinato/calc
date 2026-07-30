#include "ui/session.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include <ftxui/component/event.hpp>

#include "ui/layout.hpp"
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

}  // namespace

EditorSession::EditorSession(Document initial)
    : document(std::move(initial)), engine(document, results) {}

ftxui::Component make_view(SessionRef session, ftxui::ScreenInteractive& screen,
                           std::function<void()> on_quit) {
  auto view = ftxui::Renderer([session, &screen] {
    EditorSession& active = session();
    const auto rows = static_cast<std::size_t>(std::max(screen.dimy(), 1));
    active.viewport.height = rows > kChromeRows ? rows - kChromeRows : 1;

    apply_scroll(active.viewport, active.engine.take_scroll_request(), active.document);
    follow_cursor(active.viewport, active.document);
    active.results.refresh(active.document);
    return render_frame(active.document, active.results, active.engine, active.viewport);
  });

  // `session` is copied into the renderer above and moved into the handler here:
  // both need it, and only one of them can have the original.
  view |= ftxui::CatchEvent([session = std::move(session),
                             on_quit = std::move(on_quit)](const ftxui::Event& event) {
    const std::optional<Key> key = to_key(event);
    if (!key) return false;
    EditorSession& active = session();
    active.engine.feed(*key);
    if (active.engine.quit_requested() && on_quit) on_quit();
    return true;
  });

  return view;
}

}  // namespace calc
