// The web build's entry point. It exists because a browser tab differs from a
// terminal in exactly three ways, and every one of them lives here:
//
//   - Nothing may block. FTXUI's own Loop::Run() spins at 60fps and sleeps, which
//     would freeze the page, so this drives Loop::RunOnce() — documented as
//     non-blocking — once per animation frame instead. That is also why this
//     build needs no threads, and therefore no SharedArrayBuffer and no
//     cross-origin isolation: an iframe cannot be isolated unless the page
//     embedding it is, so a threaded build could not be embedded at all.
//   - There is no pbcopy and no xdg-open. The clipboard and the opener are
//     injected, so the web versions are two EM_ASM calls.
//   - There is nowhere to quit to. ':q' starts a fresh session rather than
//     leaving a dead terminal on the page.
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
  // Rejections are logged rather than reported in the status line: the write is
  // asynchronous and the frame that would show the message is already drawn.
  // Safari asks for a user gesture and this runs a frame after the keypress, so
  // there it may refuse; Chrome and Firefox allow it.
  // clang-format off
  EM_ASM({
    navigator.clipboard.writeText(UTF8ToString($0))
      .catch(function(error) { console.warn("calc: clipboard refused:", error); });
  }, text.c_str());
  // clang-format on
}

void open_in_browser(const std::string& url) {
  if (!is_safe_url(url)) return;

  // clang-format off
  EM_ASM({
    window.open(UTF8ToString($0), "_blank", "noopener");
  }, url.c_str());
  // clang-format on
}

// A fresh session on the demo buffer, wired to the two calls above.
std::unique_ptr<EditorSession> new_session() {
  auto session = std::make_unique<EditorSession>(Document::from_text(kDemoBuffer));
  // Named, so the status line reads like a file being edited rather than
  // "[no name]", and so ':w' has somewhere to write.
  session->document.set_path("demo.calc");
  session->engine.set_clipboard_writer(&copy_to_clipboard);
  session->engine.set_url_opener(&open_in_browser);
  return session;
}

// What one page holds for as long as it is open. The screen, the component and
// the loop are built once; only the session is replaced, which is why make_view()
// asks for the session per frame instead of capturing it.
struct Page {
  std::unique_ptr<EditorSession> session = new_session();
  bool restart_requested = false;
  ftxui::ScreenInteractive screen = ftxui::ScreenInteractive::Fullscreen();
  std::unique_ptr<ftxui::Loop> loop;
};

void tick(void* argument) {
  auto* page = static_cast<Page*>(argument);

  // Before the loop, never inside it: the flag is set from within CatchEvent, so
  // swapping the session there would destroy state the caller is still using.
  if (page->restart_requested) {
    page->restart_requested = false;
    page->session = new_session();
    page->session->engine.set_message("restarted after :q", false);
    // A frame is only redrawn when something invalidates it, and swapping the
    // session behind FTXUI's back does not, so the new buffer would stay
    // invisible until the next keystroke. This is the ask-for-a-repaint call:
    // posting a synthetic event instead puts it through the input path, which is
    // not what happened.
    page->screen.RequestAnimationFrame();
  }

  page->loop->RunOnce();
}

}  // namespace
}  // namespace calc

int main() {
  // Leaked on purpose. emscripten_set_main_loop_arg returns immediately and the
  // runtime keeps calling tick() after main() has returned, so nothing here may
  // live on this stack frame.
  auto* page = new calc::Page();

  // Off so that dragging in the terminal still selects text in the page. There is
  // nothing in calc to click.
  page->screen.TrackMouse(false);

  ftxui::Component view =
      calc::make_view([page]() -> calc::EditorSession& { return *page->session; },
                      page->screen, [page] { page->restart_requested = true; });

  page->loop = std::make_unique<ftxui::Loop>(&page->screen, std::move(view));

  // fps 0: use requestAnimationFrame, which is what the display is doing anyway.
  emscripten_set_main_loop_arg(calc::tick, page, 0, false);
  return 0;
}
