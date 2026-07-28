#include "ui/layout.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "doc/utf8.hpp"
#include "ui/theme.hpp"

namespace calc {
namespace {

using namespace ftxui;  // NOLINT(google-build-using-namespace) - a UI file

// Lines of context kept between the cursor and the edge of the window.
constexpr std::size_t kScrollOff = 3;

// What separates an expression from its result. Exactly the form the user sees:
// "1 + 2 = 3".
constexpr std::string_view kResultSeparator = " = ";

enum class CellStyle { Plain, Selected, Cursor };

Element style_run(const std::string& run, CellStyle style, Mode mode) {
  Element element = text(run);
  switch (style) {
    case CellStyle::Plain:
      return element;
    case CellStyle::Selected:
      return element | inverted;
    case CellStyle::Cursor:
      // The bar shape in insert mode sits between characters; the block sits on
      // one. focusCursor* also parks the terminal's real cursor here so the two
      // agree.
      return mode == Mode::Insert ? element | focusCursorBar
                                  : element | inverted | focusCursorBlock;
  }
  return element;
}

// Splits a line into runs of equal styling. Grouping keeps the element count
// near the number of distinct styles rather than the number of characters.
Elements styled_line(const std::string& text_line, bool draw_cursor,
                     std::size_t cursor_column, bool selected_row,
                     std::size_t selection_begin, std::size_t selection_end,
                     Mode mode) {
  Elements spans;
  std::string run;
  CellStyle run_style = CellStyle::Plain;

  const auto flush = [&] {
    if (run.empty()) return;
    spans.push_back(style_run(run, run_style, mode));
    run.clear();
  };

  std::size_t index = 0;
  while (index < text_line.size()) {
    const std::size_t next = utf8::next_boundary(text_line, index);

    CellStyle style = CellStyle::Plain;
    if (selected_row && index >= selection_begin && index < selection_end) {
      style = CellStyle::Selected;
    }
    if (draw_cursor && index == cursor_column) style = CellStyle::Cursor;

    if (style != run_style) {
      flush();
      run_style = style;
    }
    run += text_line.substr(index, next - index);
    index = next;
  }
  flush();

  // An empty line, or insert mode sitting one past the last character, still
  // needs somewhere to draw the cursor.
  if (draw_cursor && cursor_column >= text_line.size()) {
    spans.push_back(style_run(" ", CellStyle::Cursor, mode));
  }
  return spans;
}

Color mode_color(Mode mode) {
  switch (mode) {
    case Mode::Normal: return theme::mode_normal();
    case Mode::Insert: return theme::mode_insert();
    case Mode::Visual:
    case Mode::VisualLine: return theme::mode_visual();
    case Mode::CommandLine: return theme::mode_command();
  }
  return theme::mode_normal();
}

std::string gutter_text(std::size_t row, std::size_t width) {
  std::string number = std::to_string(row + 1);
  if (number.size() < width) number.insert(0, width - number.size(), ' ');
  return " " + number + " ";
}

// The bottom line: whatever the user most needs to see right now.
Element message_line(const Document& document, const ResultCache& results,
                     const VimEngine& engine) {
  if (engine.mode() == Mode::CommandLine) {
    return hbox({text(engine.command_line()),
                 text(" ") | inverted | focusCursorBar});
  }
  if (!engine.message().empty()) {
    return text(engine.message()) |
           color(engine.message_is_error() ? theme::error() : theme::notice());
  }
  // No message: explain why the current line has no result, if that is the
  // case. Errors never appear inline, because a half-typed expression is the
  // normal state while typing.
  const LineEval& eval = results.at(document.cursor().row);
  if (eval.error.has_value()) {
    // The column is reported in characters so it matches the ruler, and lines up
    // with what the user counts on screen rather than the byte offset.
    const std::size_t column =
        utf8::chars_before(document.line(document.cursor().row), eval.error->column) + 1;
    return hbox({
        text("col " + std::to_string(column) + ": ") | color(theme::separator_dim()),
        text(eval.error->message) | color(theme::error()),
    });
  }
  return text("");
}

Element status_line(const Document& document, const VimEngine& engine) {
  const std::string name = document.path().empty() ? "[no name]" : document.path();
  const std::string position = std::to_string(document.cursor().row + 1) + ":" +
                               std::to_string(utf8::chars_before(
                                                  document.line(document.cursor().row),
                                                  document.cursor().column) +
                                              1);

  Elements left;
  left.push_back(text(" " + std::string(mode_name(engine.mode())) + " ") |
                 bgcolor(mode_color(engine.mode())) | color(Color::Black) | bold);
  left.push_back(text(" " + name));
  if (document.modified()) left.push_back(text(" [+]") | color(theme::error()));

  Elements right;
  if (!engine.pending_keys().empty()) {
    right.push_back(text(engine.pending_keys() + "  ") | color(theme::notice()));
  }
  right.push_back(text(position + " "));

  return hbox({hbox(std::move(left)), filler(), hbox(std::move(right))});
}

}  // namespace

void follow_cursor(Viewport& viewport, const Document& document) {
  if (viewport.height == 0) return;

  const std::size_t row = document.cursor().row;
  const std::size_t margin = std::min(kScrollOff, viewport.height / 2);

  if (row < viewport.top_row + margin) {
    viewport.top_row = row >= margin ? row - margin : 0;
  }
  if (row + margin >= viewport.top_row + viewport.height) {
    const std::size_t wanted = row + margin + 1;
    viewport.top_row = wanted >= viewport.height ? wanted - viewport.height : 0;
  }

  // Never scroll past the last line.
  const std::size_t max_top = document.line_count() > viewport.height
                                  ? document.line_count() - viewport.height
                                  : 0;
  viewport.top_row = std::min(viewport.top_row, max_top);
  if (row < viewport.top_row) viewport.top_row = row;
}

void apply_scroll(Viewport& viewport, ScrollRequest request, Document& document) {
  const std::size_t half = std::max<std::size_t>(viewport.height / 2, 1);
  const Cursor cursor = document.cursor();

  switch (request) {
    case ScrollRequest::None:
      return;
    case ScrollRequest::HalfPageDown:
      document.set_cursor(Cursor{std::min(cursor.row + half, document.last_row()),
                                 cursor.column});
      return;
    case ScrollRequest::HalfPageUp:
      document.set_cursor(Cursor{cursor.row >= half ? cursor.row - half : 0,
                                 cursor.column});
      return;
    case ScrollRequest::LineDown:
      viewport.top_row = std::min(viewport.top_row + 1, document.last_row());
      return;
    case ScrollRequest::LineUp:
      if (viewport.top_row > 0) --viewport.top_row;
      return;
    case ScrollRequest::Center:
      viewport.top_row = cursor.row >= half ? cursor.row - half : 0;
      return;
    case ScrollRequest::Top:
      viewport.top_row = cursor.row;
      return;
    case ScrollRequest::Bottom:
      viewport.top_row = cursor.row + 1 >= viewport.height
                             ? cursor.row + 1 - viewport.height
                             : 0;
      return;
  }
}

ftxui::Element render_frame(const Document& document, const ResultCache& results,
                            const VimEngine& engine, const Viewport& viewport) {
  const Cursor cursor = document.cursor();
  const auto selection = engine.selection();
  const bool linewise_selection = engine.mode() == Mode::VisualLine;
  // In command-line mode the cursor belongs on the command line, not the buffer.
  const bool cursor_in_buffer = engine.mode() != Mode::CommandLine;

  const std::size_t gutter_width = std::to_string(document.line_count()).size();

  Elements rows;
  for (std::size_t offset = 0; offset < viewport.height; ++offset) {
    const std::size_t row = viewport.top_row + offset;

    if (row >= document.line_count()) {
      // Past the end of the buffer, like vim's empty-line markers.
      rows.push_back(text("~") | color(theme::separator_dim()));
      continue;
    }

    Elements spans;
    if (engine.line_numbers()) {
      spans.push_back(text(gutter_text(row, gutter_width)) |
                      color(row == cursor.row ? theme::gutter_current() : theme::gutter()));
    }

    std::size_t selection_begin = 0;
    std::size_t selection_end = 0;
    bool selected_row = false;
    if (selection && row >= selection->first.row && row <= selection->second.row) {
      selected_row = true;
      if (linewise_selection) {
        selection_end = document.line_length(row);
      } else {
        selection_begin = row == selection->first.row ? selection->first.column : 0;
        const std::size_t last = row == selection->second.row
                                     ? selection->second.column
                                     : document.line_length(row);
        selection_end = row == selection->second.row
                            ? utf8::next_boundary(document.line(row), last)
                            : last;
      }
    }

    Elements line_spans =
        styled_line(document.line(row), cursor_in_buffer && row == cursor.row,
                    cursor.column, selected_row, selection_begin, selection_end,
                    engine.mode());
    spans.insert(spans.end(), line_spans.begin(), line_spans.end());

    // The result. It is drawn here and stored nowhere, which is exactly why it
    // cannot be edited.
    const LineEval& eval = results.at(row);
    if (eval.has_result()) {
      spans.push_back(text(std::string(kResultSeparator)) |
                      color(theme::separator_dim()));
      spans.push_back(text(eval.text) | color(theme::result()) | bold);
    }

    rows.push_back(hbox(std::move(spans)));
  }

  return vbox({
      vbox(std::move(rows)),
      separatorLight() | color(theme::separator_dim()),
      status_line(document, engine),
      message_line(document, results, engine),
  });
}

}  // namespace calc
