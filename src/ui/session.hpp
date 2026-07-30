#pragma once

#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "ui/layout.hpp"
#include "vim/engine.hpp"

namespace calc {

// One buffer being edited, and everything derived from it. Member order is load
// bearing: the engine binds references to the two members declared above it.
struct EditorSession {
  explicit EditorSession(Document initial);

  Document document;
  ResultCache results;
  VimEngine engine;
  Viewport viewport;
};

// How a view reaches the session it draws. It asks per frame instead of holding a
// reference, so a caller can swap in a fresh session without rebuilding the
// component tree — which is how the web build restarts after ':q'.
using SessionRef = std::function<EditorSession&()>;

// The component that draws a session and feeds it keys. `on_quit` runs when a
// ':q' family command asks to leave: the terminal build exits the loop, the web
// build starts a new session, because a browser tab has nowhere to exit to.
ftxui::Component make_view(SessionRef session, ftxui::ScreenInteractive& screen,
                           std::function<void()> on_quit);

}  // namespace calc
