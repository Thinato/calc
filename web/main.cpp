#include <memory>
#include <string>
#include <utility>

#include <emscripten.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "doc/document.hpp"
#include "ui/session.hpp"
#include "ui/url.hpp"
#include "web/demo_buffer.hpp"

namespace calc {
namespace {

void copy_to_clipboard(const std::string& text) {
  EM_ASM({
    navigator.clipboard.writeText(UTF8ToString($0))
      .catch(function(error) { console.warn("calc: clipboard refused:", error); });
  }, text.c_str());
}

void open_in_browser(const std::string& url) {
  if (!is_safe_url(url)) return;

  EM_ASM({
    window.open(UTF8ToString($0), "_blank", "noopener");
  }, url.c_str());
}

std::unique_ptr<EditorSession> new_session() {
  auto session = std::make_unique<EditorSession>(Document::from_text(kDemoBuffer));
  session->document.set_path("demo.calc");
  session->engine.set_clipboard_writer(&copy_to_clipboard);
  session->engine.set_url_opener(&open_in_browser);
  return session;
}

struct Page {
  std::unique_ptr<EditorSession> session = new_session();
  bool restart_requested = false;
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
  std::unique_ptr<ftxui::Loop> loop;
};

void tick(void* argument) {
  auto* page = static_cast<Page*>(argument);

  if (page->restart_requested) {
    page->restart_requested = false;
    page->session = new_session();
    page->session->engine.set_message("restarted after :q", false);
    page->screen.RequestAnimationFrame();
  }

  page->loop->RunOnce();
}

}
}

int main() {
  auto* page = new calc::Page();

  page->screen.TrackMouse(false);

  ftxui::Component view =
      calc::make_view([page]() -> calc::EditorSession& { return *page->session; },
                      page->screen, [page] { page->restart_requested = true; });

  page->loop = std::make_unique<ftxui::Loop>(&page->screen, std::move(view));

  emscripten_set_main_loop_arg(calc::tick, page, 0, false);
  return 0;
}
