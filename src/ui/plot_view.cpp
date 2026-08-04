#include "ui/plot_view.hpp"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/dom/canvas.hpp>

#include "core/format.hpp"
#include "ui/theme.hpp"

namespace calc {
namespace {

using namespace ftxui;

constexpr std::size_t kLabelLimit = 9;

std::string axis_label(Value value) {
  std::string text = format_value(value);
  if (text.size() <= kLabelLimit) return text;

  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%.3g", value);
  return std::string(buffer);
}

std::string right_aligned(const std::string& text, std::size_t width) {
  if (text.size() >= width) return text;
  return std::string(width - text.size(), ' ') + text;
}

std::optional<std::string> plot_problem(const PlotSpec& spec,
                                        const Environment& environment) {
  const std::optional<std::size_t> arity = plot_arity(spec.name, environment);
  if (!arity.has_value()) return "unknown function '" + spec.name + "'";
  if (*arity != 1) {
    return spec.name + "() takes " + std::to_string(*arity) +
           (*arity == 1 ? " argument" : " arguments");
  }
  return std::nullopt;
}

int dot_column_for(std::size_t index, int dot_columns) {
  if (dot_columns <= 1 || kPlotSamples <= 1) return 0;
  const std::size_t span = static_cast<std::size_t>(dot_columns - 1);
  return static_cast<int>(index * span / (kPlotSamples - 1));
}

void draw_axes(Canvas& canvas, const PlotData& data, const PlotSpec& spec) {
  const int dot_rows = canvas.height();
  const int dot_columns = canvas.width();

  const int zero_row = plot_dot_row(data, 0, dot_rows);
  if (zero_row >= 0 && zero_row < dot_rows) {
    for (int x = 0; x < dot_columns; x += 2) {
      canvas.DrawPoint(x, zero_row, true, theme::separator_dim());
    }
  }

  const Value span = spec.x_max - spec.x_min;
  if (span <= 0) return;
  const Value fraction = (0 - spec.x_min) / span;
  if (fraction < 0 || fraction > 1) return;

  const int zero_column =
      static_cast<int>(fraction * static_cast<Value>(dot_columns - 1));
  for (int y = 0; y < dot_rows; y += 2) {
    canvas.DrawPoint(zero_column, y, true, theme::separator_dim());
  }
}

void draw_curve(Canvas& canvas, const PlotData& data) {
  const int dot_rows = canvas.height();
  const int dot_columns = canvas.width();

  int previous_column = 0;
  int previous_row = 0;
  bool joined = false;

  for (std::size_t index = 0; index < data.samples.size(); ++index) {
    const PlotSample& sample = data.samples[index];
    if (!sample.ok) {
      joined = false;
      continue;
    }

    const int column = dot_column_for(index, dot_columns);
    const int row = plot_dot_row(data, sample.y, dot_rows);

    if (joined) {
      canvas.DrawPointLine(previous_column, previous_row, column, row, theme::result());
    } else {
      canvas.DrawPoint(column, row, true, theme::result());
    }

    previous_column = column;
    previous_row = row;
    joined = true;
  }
}

Element title_row(const PlotSpec& spec, const PlotData& data,
                  const std::optional<std::string>& problem) {
  Elements spans;
  spans.push_back(text(" " + spec.name) | color(theme::function()) | bold);

  if (problem.has_value()) {
    spans.push_back(text("  " + *problem) | color(theme::error()));
  } else if (!data.has_points) {
    const std::string reason =
        data.first_error.has_value() ? data.first_error->message : "nothing to draw here";
    spans.push_back(text("  " + reason) | color(theme::error()));
  } else {
    spans.push_back(text("  x " + axis_label(spec.x_min) + ".." + axis_label(spec.x_max) +
                         "  y " + axis_label(data.y_min) + ".." +
                         axis_label(data.y_max)) |
                    color(theme::separator_dim()));
  }

  spans.push_back(filler());
  return hbox(std::move(spans));
}

Element y_labels(const PlotData& data, std::size_t canvas_rows, std::size_t width) {
  const std::size_t dot_rows = canvas_rows * 4;
  const int zero_row = plot_dot_row(data, 0, static_cast<int>(dot_rows));
  const std::size_t zero_cell =
      zero_row >= 0 ? static_cast<std::size_t>(zero_row) / 4 : canvas_rows;

  Elements rows;
  for (std::size_t row = 0; row < canvas_rows; ++row) {
    std::string label;
    if (row == 0) {
      label = axis_label(data.y_max);
    } else if (row + 1 == canvas_rows) {
      label = axis_label(data.y_min);
    } else if (row == zero_cell) {
      label = "0";
    }
    rows.push_back(text(right_aligned(label, width) + " ") |
                   color(theme::separator_dim()));
  }
  return vbox(std::move(rows));
}

Element x_labels(const PlotSpec& spec, std::size_t gutter) {
  return hbox({
      text(std::string(gutter, ' ')),
      text(axis_label(spec.x_min)) | color(theme::separator_dim()),
      filler(),
      text(axis_label(spec.x_max)) | color(theme::separator_dim()),
  });
}

}

ftxui::Element plot_panel(const PlotSpec& spec, const Environment& environment,
                          std::size_t rows) {
  if (rows <= kPlotPanelChrome) return text("");

  const std::optional<std::string> problem = plot_problem(spec, environment);

  PlotData data;
  if (problem.has_value()) {
    data.y_min = -1;
    data.y_max = 1;
  } else {
    data = sample_plot(spec, environment);
  }

  const std::size_t canvas_rows = rows - kPlotPanelChrome;
  const std::size_t gutter =
      std::max(axis_label(data.y_min).size(), axis_label(data.y_max).size()) + 1;

  auto draw = [data, spec](Canvas& canvas) {
    if (!data.has_points || canvas.width() <= 0 || canvas.height() <= 0) return;
    draw_axes(canvas, data, spec);
    draw_curve(canvas, data);
  };

  return vbox({
      separatorLight() | color(theme::separator_dim()),
      title_row(spec, data, problem),
      hbox({
          y_labels(data, canvas_rows, gutter - 1),
          canvas(2, 4, draw) | flex,
      }) | size(HEIGHT, EQUAL, static_cast<int>(canvas_rows)),
      x_labels(spec, gutter),
  });
}

}
