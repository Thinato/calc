#include <doctest/doctest.h>

#include <string>
#include <vector>

#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/terminal.hpp>

#include "helpers/vim_harness.hpp"
#include "ui/layout.hpp"
#include "ui/theme.hpp"

using namespace calc;

namespace {

// FTXUI decides colour support from TERM and COLORTERM, and downgrades a
// 256-colour value to its nearest 16-colour index when it doubts the terminal.
// Pinned here, before any Color is constructed, so that what the colour
// assertions below see does not depend on whose shell ran ctest.
struct ColorSupportPin {
  ColorSupportPin() {
    ftxui::Terminal::SetColorSupport(ftxui::Terminal::Palette256);
  }
};
const ColorSupportPin kColorSupportPin;

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

// Tells a foreground code apart from the background and attribute codes FTXUI
// emits alongside it — it writes both colours on any change, so an escape
// sequence on its own says little.
bool is_foreground(const std::string& code) {
  if (code == "39" || code == "0") return true;  // back to the terminal's default
  if (code.rfind("38;", 0) == 0) return true;    // 256-colour or true colour
  if (code.size() != 2) return false;            // an attribute, not a colour
  return (code[0] == '3' || code[0] == '9') && code[1] >= '0' && code[1] <= '7';
}

// The foreground colour in effect where `needle` begins, as the SGR payload
// ("38;5;247", or "39" for the terminal's own default). Colour is exactly what
// plain() throws away; asserting on it by position rather than by matching whole
// escape sequences keeps these tests indifferent to how FTXUI groups and orders
// them.
std::string color_at(const std::vector<std::string>& lines, std::string_view needle) {
  for (const std::string& line : lines) {
    std::string visible;
    std::vector<std::string> foreground;  // one entry per byte of `visible`
    std::string current = "39";

    for (std::size_t index = 0; index < line.size();) {
      if (line[index] == '\x1B') {
        const std::size_t end = line.find('m', index);
        if (end == std::string::npos) break;
        const std::string code = line.substr(index + 2, end - index - 2);
        // "0" clears every attribute, the foreground among them, and is how FTXUI
        // ends a bold run — so it has to count as a colour change, not be skipped
        // as an attribute.
        if (is_foreground(code)) current = code == "0" ? "39" : code;
        index = end + 1;
        continue;
      }
      visible += line[index++];
      foreground.push_back(current);
    }

    const std::size_t at = visible.find(needle);
    if (at != std::string::npos) return foreground[at];
  }
  return "[not on screen]";
}

// What the theme asked for, in the form the screen writes it.
std::string expected(ftxui::Color color) { return color.Print(false); }

constexpr std::string_view kTerminalDefault = "39";

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

TEST_CASE("a half-typed expression stays quiet under the cursor") {
  // The reason the current line is the one exception: mid-typing is the normal
  // state, and being told about it on every keystroke is noise.
  const auto lines = render_lines("", "i1 + ");
  CHECK_FALSE(contains(lines, "="));
  CHECK_FALSE(contains(lines, "Error:"));
}

TEST_CASE("an unfinished line reports itself once the cursor leaves") {
  const auto lines = render_lines("", "i1 +<esc>o", 60);
  CHECK(contains(lines, "1 +  Error: incomplete expression"));
}

TEST_CASE("a definition renders its computed value after the expression") {
  const auto lines = render_lines("x = 1 + 2", "");
  CHECK(contains(lines, "x = 1 + 2 = 3"));
}

TEST_CASE("a definition with a typed value gains no second '='") {
  const auto lines = render_lines("subtotal = 128.40", "", 60);
  CHECK(contains(lines, "subtotal = 128.40"));
  // Neither restated nor silently reformatted to 128.4.
  CHECK_FALSE(contains(lines, "128.40 ="));
  CHECK_FALSE(contains(lines, "128.4 "));
}

TEST_CASE("the worked example renders as designed") {
  const auto lines = render_lines(
      "subtotal = 128.40\nRATE = 0.0825\ntip = subtotal * 0.2\n"
      "subtotal * RATE\nsubtotal + tip",
      "", 60, 12);
  CHECK(contains(lines, "subtotal = 128.40"));
  CHECK(contains(lines, "RATE = 0.0825"));
  CHECK(contains(lines, "tip = subtotal * 0.2 = 25.68"));
  CHECK(contains(lines, "subtotal * RATE = 10.593"));
  CHECK(contains(lines, "subtotal + tip = 154.08"));
}

TEST_CASE("a name used before it is defined says so beside the line") {
  const auto lines = render_lines("x * 2\nx = 5", "j", 60);
  CHECK(contains(lines, "x * 2  Error: undefined name 'x'"));
}

TEST_CASE("reassigning a constant reports where it came from") {
  // Note the cursor stays on line 1: the error is on line 2, and parking on it
  // would hide it.
  const auto lines = render_lines("TEST = 2\nTEST = 5", "", 60);
  CHECK(contains(lines, "Error: TEST is a constant, defined on line 1"));
}

TEST_CASE("an error is drawn after the line that caused it") {
  const auto lines = render_lines("10 + 1 / 0\nx = 1", "j", 60);
  CHECK(contains(lines, "10 + 1 / 0  Error: division by zero"));
  // The column is gone: beside the expression it no longer earns its width.
  CHECK_FALSE(contains(lines, "col 8"));
}

TEST_CASE("an error is hidden while the cursor is on its line") {
  const auto lines = render_lines("10 + 1 / 0\nx = 1", "", 60);
  CHECK_FALSE(contains(lines, "Error:"));
  CHECK_FALSE(contains(lines, "division by zero"));
}

TEST_CASE("moving back onto an erroring line hides it again") {
  CHECK(contains(render_lines("10 + 1 / 0\nx = 1", "j", 60), "Error:"));
  CHECK_FALSE(contains(render_lines("10 + 1 / 0\nx = 1", "jk", 60), "Error:"));
}

TEST_CASE("a line never shows both a result and an error") {
  // The two overlays occupy the same place, and evaluate_line() guarantees a
  // line has at most one of them.
  const auto lines = render_lines("1 / 0\n1 + 2", "j", 60);
  CHECK(contains(lines, "1 / 0  Error: division by zero"));
  CHECK_FALSE(contains(lines, "1 / 0 ="));
  CHECK(contains(lines, "1 + 2 = 3"));      // and a good line is untouched
  CHECK_FALSE(contains(lines, "1 + 2  Error"));
}

TEST_CASE("a long error clips itself rather than the expression") {
  // The overlay must never cost the user characters they typed. 30 columns is
  // far too narrow for this message, so something has to give.
  const auto lines = render_lines("undefined_name * 2\nx = 1", "j", 30);
  CHECK(contains(lines, "undefined_name * 2  Error: "));
  CHECK_FALSE(contains(lines, "undefined_name'"));  // the message lost its tail
}

TEST_CASE("a comment is drawn in the comment colour") {
  // Its '#' included, on a line the cursor is not on, which is prose's ordinary
  // state.
  const auto lines = render_lines("# just a note\n1 + 2", "j");
  CHECK(color_at(lines, "# just a note") == expected(theme::comment()));
  // And the expression below it is left in the terminal's own foreground.
  CHECK(color_at(lines, "1 + 2") == kTerminalDefault);
}

TEST_CASE("a comment reads as quiet content, not as chrome") {
  // The point of the shade: dimmer than what you typed, but not the same dim as
  // the line numbers, or the eye cannot separate the two.
  const auto lines = render_lines("# just a note\n1 + 2", "j");
  CHECK(color_at(lines, " 1 ") == expected(theme::gutter()));  // the gutter beside it
  CHECK(color_at(lines, "# just a note") != color_at(lines, " 1 "));
  CHECK(color_at(lines, "# just a note") != kTerminalDefault);
}

TEST_CASE("a function name is drawn in the function colour") {
  // The cursor starts on the 's' and would occupy that cell, so it moves away.
  const auto lines = render_lines("sqrt(16) + pow(2, 10)\n1 + 2", "j", 60);
  CHECK(color_at(lines, "sqrt") == expected(theme::function()));
  CHECK(color_at(lines, "pow") == expected(theme::function()));
  // Only the name: the parentheses and the arguments stay plain, so what stands
  // out is the thing being called.
  CHECK(color_at(lines, "(16)") == kTerminalDefault);
  CHECK(color_at(lines, "2, 10") == kTerminalDefault);
}

TEST_CASE("a function does not read as its own result") {
  // Light blue against the cyan result. If these matched, `sqrt(16) = 4` would
  // look like the line restating part of itself.
  const auto lines = render_lines("sqrt(16)", "");
  CHECK(color_at(lines, "4") == expected(theme::result()));
  CHECK(color_at(lines, "sqrt") != color_at(lines, "4"));
}

TEST_CASE("a function named inside a comment is not coloured as a call") {
  const auto lines = render_lines("# use sqrt for that\n1 + 2", "j", 60);
  CHECK(color_at(lines, "sqrt") == expected(theme::comment()));
}

TEST_CASE("the cursor still shows inside a comment") {
  // Syntax sits at the bottom of the precedence order: knowing where you are
  // matters more than what you are looking at.
  const auto lines = render_lines("# a note", "");
  CHECK(color_at(lines, "#") == kTerminalDefault);  // the cursor cell, inverted
  CHECK(color_at(lines, " a note") == expected(theme::comment()));
}

TEST_CASE("a definition keeps its own colour beside the new ones") {
  // The cursor moves off the name first, or it would cover the character these
  // look at.
  CHECK(color_at(render_lines("subtotal = 128.40\n1 + 2", "j", 60), "subtotal") ==
        expected(theme::variable()));
  CHECK(color_at(render_lines("RATE = 0.0825\n1 + 2", "j", 60), "RATE") ==
        expected(theme::constant()));
}

TEST_CASE("a function name cannot be redefined, and stays coloured as one") {
  // The colour and the error agree: a function name is not available to define,
  // so it is still a function here rather than a new variable.
  const auto lines = render_lines("sqrt = 5\n1 + 2", "j", 60);
  CHECK(color_at(lines, "sqrt") == expected(theme::function()));
  CHECK(contains(lines, "Error: 'sqrt' is a function"));
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
