#include "ui/layout.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/syntax.hpp"
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

// And what introduces an error, drawn in the same place a result would be. Two
// leading spaces because an error has no " = " to glue it to the expression, and
// without the gap "1 +Error:" reads as one token.
constexpr std::string_view kErrorPrefix = "  Error: ";

// Ordered by precedence: a cursor or a selection wins over a name highlight,
// because knowing where you are matters more than what a name is. Syntax sits at
// the bottom, though the two never actually compete over the same characters — a
// definition cannot be named after a function (Environment::define rejects that,
// environment.cpp:51) and a name inside a comment is not a definition.
enum class CellStyle { Plain, Comment, Function, Variable, Constant, Selected, Cursor };

Element style_run(const std::string& run, CellStyle style, Mode mode) {
  Element element = text(run);
  switch (style) {
    case CellStyle::Plain: return element;
    case CellStyle::Comment: return element | color(theme::comment());
    case CellStyle::Function:
      // No bold: weight is how a constant and the two overlays stand out, and
      // spending it here would leave nothing to stand out against.
      return element | color(theme::function());
    case CellStyle::Variable: return element | color(theme::variable());
    case CellStyle::Constant: return element | color(theme::constant()) | bold;
    case CellStyle::Selected: return element | inverted;
    case CellStyle::Cursor:
      // The bar shape in insert mode sits between characters; the block sits on
      // one. focusCursor* also parks the terminal's real cursor here so the two
      // agree.
      return mode == Mode::Insert ? element | focusCursorBar
                                  : element | inverted | focusCursorBlock;
  }
  return element;
}

// Everything that decides how one line's characters are drawn. A struct rather
// than a parameter list because the three overlapping concerns — cursor,
// selection, name highlight — would otherwise be seven positional arguments.
struct LineStyling {
  Mode mode = Mode::Normal;
  // What the line is made of: comments and function names. Ascending and
  // non-overlapping, so walking it alongside the characters needs no search.
  std::vector<SyntaxSpan> syntax;
  bool draw_cursor = false;
  std::size_t cursor_column = 0;
  bool selected_row = false;
  std::size_t selection_begin = 0;
  std::size_t selection_end = 0;
  // The name an assignment defines, so definitions stand out from uses.
  std::size_t name_begin = 0;
  std::size_t name_end = 0;
  bool name_is_constant = false;
};

// Splits a line into runs of equal styling. Grouping keeps the element count
// near the number of distinct styles rather than the number of characters.
Elements styled_line(const std::string& text_line, const LineStyling& styling) {
  Elements spans;
  std::string run;
  CellStyle run_style = CellStyle::Plain;

  const auto flush = [&] {
    if (run.empty()) return;
    spans.push_back(style_run(run, run_style, styling.mode));
    run.clear();
  };

  std::size_t index = 0;
  std::size_t span = 0;
  while (index < text_line.size()) {
    const std::size_t next = utf8::next_boundary(text_line, index);

    CellStyle style = CellStyle::Plain;
    // The spans ascend, so one forward-only cursor into them keeps this a single
    // pass rather than a search per character.
    while (span < styling.syntax.size() && styling.syntax[span].end <= index) ++span;
    if (span < styling.syntax.size() && index >= styling.syntax[span].begin) {
      style = styling.syntax[span].kind == SyntaxKind::Comment ? CellStyle::Comment
                                                               : CellStyle::Function;
    }
    if (index >= styling.name_begin && index < styling.name_end) {
      style = styling.name_is_constant ? CellStyle::Constant : CellStyle::Variable;
    }
    if (styling.selected_row && index >= styling.selection_begin &&
        index < styling.selection_end) {
      style = CellStyle::Selected;
    }
    if (styling.draw_cursor && index == styling.cursor_column) {
      style = CellStyle::Cursor;
    }

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
  if (styling.draw_cursor && styling.cursor_column >= text_line.size()) {
    spans.push_back(style_run(" ", CellStyle::Cursor, styling.mode));
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

// The bottom line: the command being typed, or the answer to something the user
// just did — ":nonsense", a failed search, "yanked 3". A line's own error is a
// property of that line and is drawn beside it instead, so nothing here reports
// on the buffer's contents any more.
Element message_line(const VimEngine& engine) {
  if (engine.mode() == Mode::CommandLine) {
    return hbox({text(engine.command_line()), text(" ") | inverted | focusCursorBar});
  }
  if (!engine.message().empty()) {
    return text(engine.message()) |
           color(engine.message_is_error() ? theme::error() : theme::notice());
  }
  return text("");
}

Element status_line(const Document& document, const VimEngine& engine) {
  const std::string name = document.path().empty() ? "[no name]" : document.path();
  const std::string position =
      std::to_string(document.cursor().row + 1) + ":" +
      std::to_string(utf8::chars_before(document.line(document.cursor().row),
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
    case ScrollRequest::None: return;
    case ScrollRequest::HalfPageDown:
      document.set_cursor(
          Cursor{std::min(cursor.row + half, document.last_row()), cursor.column});
      return;
    case ScrollRequest::HalfPageUp:
      document.set_cursor(
          Cursor{cursor.row >= half ? cursor.row - half : 0, cursor.column});
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
    case ScrollRequest::Top: viewport.top_row = cursor.row; return;
    case ScrollRequest::Bottom:
      viewport.top_row =
          cursor.row + 1 >= viewport.height ? cursor.row + 1 - viewport.height : 0;
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
      spans.push_back(
          text(gutter_text(row, gutter_width)) |
          color(row == cursor.row ? theme::gutter_current() : theme::gutter()));
    }

    const LineEval& eval = results.at(row);

    LineStyling styling;
    styling.mode = engine.mode();
    // Recomputed per visible row per frame: one scan of one line, for the twenty
    // or so lines actually on screen. Caching it in ResultCache would tie an
    // evaluation cache to a drawing concern for no measurable gain.
    styling.syntax = syntax_spans(document.line(row));
    styling.draw_cursor = cursor_in_buffer && row == cursor.row;
    styling.cursor_column = cursor.column;

    if (selection && row >= selection->first.row && row <= selection->second.row) {
      styling.selected_row = true;
      if (linewise_selection) {
        styling.selection_end = document.line_length(row);
      } else {
        styling.selection_begin =
            row == selection->first.row ? selection->first.column : 0;
        const std::size_t last = row == selection->second.row ? selection->second.column
                                                              : document.line_length(row);
        styling.selection_end = row == selection->second.row
                                    ? utf8::next_boundary(document.line(row), last)
                                    : last;
      }
    }

    // Bound in the condition so the check and the use are visibly the same
    // optional; going through eval.is_assignment() reads well but hides that from
    // an analyser, which then cannot tell this dereference is guarded.
    if (const std::optional<std::string>& name = eval.assigned_name; name.has_value()) {
      styling.name_begin = eval.assigned_column;
      styling.name_end = eval.assigned_column + name->size();
      styling.name_is_constant = eval.assigned_constant;
    }

    Elements line_spans = styled_line(document.line(row), styling);
    spans.insert(spans.end(), line_spans.begin(), line_spans.end());

    // The result, or the reason there is none. Both are drawn here and stored
    // nowhere, which is exactly why neither can be edited. A definition whose
    // value was typed out literally shows nothing, so `x = 128.40` is never
    // restated as `= 128.4`.
    //
    // They are mutually exclusive by construction — a failed evaluate_line()
    // returns before it sets a value — so `else if` states that invariant rather
    // than merely ordering two independent checks.
    //
    // An error stays hidden on the line the cursor is on, because a half-typed
    // expression is the normal state while editing; it appears the moment you
    // move away. That keys off the row rather than styling.draw_cursor, which is
    // false in command-line mode: pressing ':' should not flicker an error into
    // view on the line you are still parked on.
    if (eval.has_result() && eval.show_result) {
      spans.push_back(text(std::string(kResultSeparator)) |
                      color(theme::separator_dim()));
      spans.push_back(text(eval.text) | color(theme::result()) | bold);
    } else if (eval.error.has_value() && row != cursor.row) {
      spans.push_back(text(std::string(kErrorPrefix)) | color(theme::error()) | bold);
      // The message is the only span allowed to shrink. An hbox with nothing
      // shrinkable narrows every child in proportion, which on a narrow window
      // would eat the user's own text to make room for our overlay — exactly
      // backwards. Marking the message means it absorbs the whole shortfall, so
      // the expression keeps its width and a clipped message stays obviously
      // clipped. Deliberately not applied to a result: a truncated number would
      // read as a wrong answer rather than a partial one.
      spans.push_back(text(eval.error->message) | color(theme::error()) | xflex_shrink);
    }

    rows.push_back(hbox(std::move(spans)));
  }

  return vbox({
      vbox(std::move(rows)),
      separatorLight() | color(theme::separator_dim()),
      status_line(document, engine),
      message_line(engine),
  });
}

}  // namespace calc
