#include "ui/layout.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "core/syntax.hpp"
#include "doc/utf8.hpp"
#include "ui/plot_view.hpp"
#include "ui/theme.hpp"

namespace calc {
namespace {

using namespace ftxui;

constexpr std::size_t kScrollOff = 3;

constexpr std::string_view kResultSeparator = " = ";

constexpr std::string_view kErrorPrefix = "  Error: ";

enum class CellStyle {
  Plain,
  Comment,
  Function,
  Keyword,
  Variable,
  Constant,
  Selected,
  Cursor
};

Element style_run(const std::string& run, CellStyle style, Mode mode) {
  Element element = text(run);
  switch (style) {
    case CellStyle::Plain: return element;
    case CellStyle::Comment: return element | color(theme::comment());
    case CellStyle::Function: return element | color(theme::function());
    case CellStyle::Keyword: return element | bold;
    case CellStyle::Variable: return element | color(theme::variable());
    case CellStyle::Constant: return element | color(theme::constant()) | bold;
    case CellStyle::Selected: return element | inverted;
    case CellStyle::Cursor:
      return mode == Mode::Insert ? element | focusCursorBar
                                  : element | inverted | focusCursorBlock;
  }
  return element;
}

struct LineStyling {
  Mode mode = Mode::Normal;
  std::vector<SyntaxSpan> syntax;
  bool draw_cursor = false;
  std::size_t cursor_column = 0;
  bool selected_row = false;
  std::size_t selection_begin = 0;
  std::size_t selection_end = 0;
  std::size_t name_begin = 0;
  std::size_t name_end = 0;
  bool name_is_constant = false;
};

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
    while (span < styling.syntax.size() && styling.syntax[span].end <= index) ++span;
    if (span < styling.syntax.size() && index >= styling.syntax[span].begin) {
      switch (styling.syntax[span].kind) {
        case SyntaxKind::Comment: style = CellStyle::Comment; break;
        case SyntaxKind::Function: style = CellStyle::Function; break;
        case SyntaxKind::Keyword: style = CellStyle::Keyword; break;
      }
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

}

std::size_t plot_rows(std::size_t screen_rows, bool plot_open) {
  if (!plot_open || screen_rows <= kChromeRows + kPlotBufferFloor) return 0;

  const std::size_t available = screen_rows - kChromeRows - kPlotBufferFloor;
  const std::size_t wanted = std::max(screen_rows * 2 / 5, kPlotMinRows);
  const std::size_t rows = std::min(wanted, available);
  return rows < kPlotPanelChrome + 1 ? 0 : rows;
}

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
  const bool cursor_in_buffer = engine.mode() != Mode::CommandLine;

  const std::size_t gutter_width = std::to_string(document.line_count()).size();

  Elements rows;
  for (std::size_t offset = 0; offset < viewport.height; ++offset) {
    const std::size_t row = viewport.top_row + offset;

    if (row >= document.line_count()) {
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
    styling.syntax =
        syntax_spans(document.line(row), [&results, row](std::string_view name) {
          return results.is_function_at(name, row);
        });
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

    if (const std::optional<std::string>& name = eval.assigned_name; name.has_value()) {
      styling.name_begin = eval.assigned_column;
      styling.name_end = eval.assigned_column + name->size();
      styling.name_is_constant = eval.assigned_constant;
    }

    Elements line_spans = styled_line(document.line(row), styling);
    spans.insert(spans.end(), line_spans.begin(), line_spans.end());

    if (eval.has_result() && eval.show_result) {
      spans.push_back(text(std::string(kResultSeparator)) |
                      color(theme::separator_dim()));
      spans.push_back(text(eval.text) | color(theme::result()) | bold);
    } else if (eval.error.has_value() && row != cursor.row) {
      spans.push_back(text(std::string(kErrorPrefix)) | color(theme::error()) | bold);
      spans.push_back(text(eval.error->message) | color(theme::error()) | xflex_shrink);
    }

    rows.push_back(hbox(std::move(spans)));
  }

  Elements frame;
  frame.push_back(vbox(std::move(rows)));
  if (engine.plot().has_value() && viewport.plot_height > kPlotPanelChrome) {
    frame.push_back(
        plot_panel(*engine.plot(), results.environment(), viewport.plot_height));
  }
  frame.push_back(separatorLight() | color(theme::separator_dim()));
  frame.push_back(status_line(document, engine));
  frame.push_back(message_line(engine));

  return vbox(std::move(frame));
}

}
