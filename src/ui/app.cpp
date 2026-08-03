#include "ui/app.hpp"

#include <cstdio>
#include <string>
#include <utility>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "ui/session.hpp"
#include "ui/url.hpp"

namespace calc {
namespace {

void copy_to_clipboard(const std::string& text) {
#if defined(__APPLE__)
  const char* command = "pbcopy";
#elif defined(__linux__)
  const char* command = "xclip -selection clipboard";
#else
  const char* command = nullptr;
#endif
  if (command == nullptr) return;

  FILE* pipe = popen(command, "w");
  if (pipe == nullptr) return;
  std::fwrite(text.data(), 1, text.size(), pipe);
  pclose(pipe);
}

void open_in_browser(const std::string& url) {
#if defined(__APPLE__)
  const char* opener = "open";
#elif defined(__linux__)
  const char* opener = "xdg-open";
#else
  const char* opener = nullptr;
#endif
  if (opener == nullptr || !is_safe_url(url)) return;

  const std::string command = std::string(opener) + " '" + url + "' >/dev/null 2>&1 &";

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe != nullptr) pclose(pipe);
}

}

int run_editor(Document document) {
  EditorSession session(std::move(document));
  session.engine.set_clipboard_writer(&copy_to_clipboard);
  session.engine.set_url_opener(&open_in_browser);

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  ftxui::Component view = make_view([&session]() -> EditorSession& { return session; },
                                    screen, [&screen] { screen.Exit(); });

  screen.Loop(view);
  return 0;
}

}
