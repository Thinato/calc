#pragma once

#include <functional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "ui/layout.hpp"
#include "vim/engine.hpp"

namespace calc {

struct EditorSession {
  explicit EditorSession(Document initial);

  Document document;
  ResultCache results;
  VimEngine engine;
  Viewport viewport;
};

using SessionRef = std::function<EditorSession&()>;

ftxui::Component make_view(SessionRef session, ftxui::ScreenInteractive& screen,
                           std::function<void()> on_quit);

}
