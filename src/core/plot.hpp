#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/environment.hpp"
#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

constexpr std::size_t kPlotSamples = 320;

struct PlotSpec {
  std::string name;
  Value x_min = -10;
  Value x_max = 10;
  bool auto_y = true;
  Value y_min = 0;
  Value y_max = 0;
};

struct PlotSample {
  Value x = 0;
  Value y = 0;
  bool ok = false;
};

struct PlotData {
  std::vector<PlotSample> samples;
  Value y_min = 0;
  Value y_max = 0;
  bool has_points = false;
  std::optional<Error> first_error;
};

std::optional<std::size_t> plot_arity(std::string_view name,
                                      const Environment& environment);

PlotData sample_plot(const PlotSpec& spec, const Environment& environment);

int plot_dot_row(const PlotData& data, Value y, int dot_rows);

}
