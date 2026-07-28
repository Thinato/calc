#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <ftxui/screen/screen.hpp>

#include "helpers/vim_harness.hpp"
#include "ui/layout.hpp"

using namespace calc;

namespace {

// Renders a frame to an off-screen buffer. No terminal is involved, which is
// what makes the layout testable at all.
std::vector<std::string> render_lines(std::string_view initial,
                                      std::string_view script, int width = 44,
                                      int height = 8) {
  Document document = Document::from_text(initial);
  ResultCache results;
  VimEngine engine(document, results);
  for (const Key& key : test::parse_keys(script)) {
    engine.feed(key);
    results.refresh(document);
  }
  results.refresh(document);

  Viewport viewport;
  viewport.height = static_cast<std::size_t>(height) > kChromeRows
                        ? static_cast<std::size_t>(height) - kChromeRows
                        : 1;
  follow_cursor(viewport, document);

  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                      ftxui::Dimension::Fixed(height));
  ftxui::Element frame = render_frame(document, results, engine, viewport);
  ftxui::Render(screen, frame);

  std::vector<std::string> lines;
  std::string rendered = screen.ToString();
  std::size_t start = 0;
  while (start <= rendered.size()) {
    const std::size_t newline = rendered.find('\n', start);
    if (newline == std::string::npos) {
      lines.push_back(rendered.substr(start));
      break;
    }
    lines.push_back(rendered.substr(start, newline - start));
    start = newline + 1;
  }
  return lines;
}

// Strips the ANSI escape sequences the screen emits for colour and inversion.
std::string plain(const std::string& text) {
  std::string result;
  for (std::size_t index = 0; index < text.size();) {
    if (text[index] == '\x1B') {
      while (index < text.size() && text[index] != 'm') ++index;
      if (index < text.size()) ++index;
      continue;
    }
    result += text[index++];
  }
  return result;
}

bool contains(const std::vector<std::string>& lines, std::string_view needle) {
  for (const std::string& line : lines) {
    if (plain(line).find(needle) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST_CASE("a result is drawn after the expression that produced it") {
  const auto lines = render_lines("1 + 2", "");
  CHECK(contains(lines, "1 + 2 = 3"));
}

TEST_CASE("the frame from the brief renders as specified") {
  const auto lines = render_lines("1 + 2\nsqrt(16) + pow(2, 10)\n(1 + 2) * 3 ^ 2", "", 60);
  CHECK(contains(lines, "1 + 2 = 3"));
  CHECK(contains(lines, "sqrt(16) + pow(2, 10) = 1028"));
  CHECK(contains(lines, "(1 + 2) * 3 ^ 2 = 27"));
}

TEST_CASE("line numbers appear in the gutter and can be turned off") {
  const auto numbered = render_lines("1 + 2", "");
  CHECK(contains(numbered, " 1 1 + 2 = 3"));

  const auto plain_gutter = render_lines("1 + 2", ":set nonumber<cr>");
  CHECK(contains(plain_gutter, "1 + 2 = 3"));
  CHECK_FALSE(contains(plain_gutter, " 1 1 + 2"));
}

TEST_CASE("a line with no result shows no separator") {
  const auto lines = render_lines("hello", "");
  CHECK(contains(lines, "hello"));
  CHECK_FALSE(contains(lines, "hello ="));
}

TEST_CASE("a half-typed expression shows no inline error") {
  // Errors belong in the status bar, because mid-typing is the normal state.
  const auto lines = render_lines("", "i1 + ");
  CHECK_FALSE(contains(lines, "="));
  CHECK(contains(lines, "incomplete expression"));
}

TEST_CASE("an error names the column it happened at") {
  const auto lines = render_lines("10 + 1 / 0", "");
  CHECK(contains(lines, "col 8: division by zero"));  // the '/'
}

TEST_CASE("the status bar names the mode and the cursor position") {
  CHECK(contains(render_lines("1 + 2", ""), "NORMAL"));
  CHECK(contains(render_lines("1 + 2", "i"), "INSERT"));
  CHECK(contains(render_lines("1 + 2", "v"), "VISUAL"));
  CHECK(contains(render_lines("1 + 2", "V"), "V-LINE"));
  CHECK(contains(render_lines("1 + 2", ":"), "COMMAND"));

  CHECK(contains(render_lines("1 + 2", ""), "1:1"));
  CHECK(contains(render_lines("1 + 2", "ll"), "1:3"));
  CHECK(contains(render_lines("a\nb", "j"), "2:1"));
}

TEST_CASE("the status bar flags unsaved changes") {
  CHECK_FALSE(contains(render_lines("1 + 2", ""), "[+]"));
  CHECK(contains(render_lines("1 + 2", "x"), "[+]"));
}

TEST_CASE("the cursor position is reported in characters, not bytes") {
  // 'é' is two bytes but one column.
  const auto lines = render_lines("# é1", "lll");
  CHECK(contains(lines, "1:4"));
}

TEST_CASE("a pending command is shown while it is being typed") {
  CHECK(contains(render_lines("a\nb\nc", "2d"), "2d"));
}

TEST_CASE("the command line is echoed as it is typed") {
  const auto lines = render_lines("1 + 2", ":w foo");
  CHECK(contains(lines, ":w foo"));
}

TEST_CASE("an error message reaches the bottom line") {
  const auto lines = render_lines("1 + 2", ":nonsense<cr>");
  CHECK(contains(lines, "not an editor command"));
}

TEST_CASE("rows past the end of the buffer are marked") {
  const auto lines = render_lines("1 + 2", "");
  CHECK(contains(lines, "~"));
}

TEST_CASE("the viewport follows the cursor through a long buffer") {
  std::string many;
  for (int index = 1; index <= 60; ++index) {
    many += std::to_string(index) + " + 0\n";
  }
  // Line 60 must be on screen after G, and line 1 must not.
  const auto lines = render_lines(many, "G", 44, 10);
  CHECK(contains(lines, "60 + 0 = 60"));
  CHECK_FALSE(contains(lines, " 1 1 + 0"));
}
