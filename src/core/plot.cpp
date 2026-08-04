#include "core/plot.hpp"

#include <cmath>
#include <utility>

#include "core/ast.hpp"
#include "core/eval.hpp"
#include "core/functions.hpp"

namespace calc {

std::optional<std::size_t> plot_arity(std::string_view name,
                                      const Environment& environment) {
  if (const FunctionDef* builtin = find_function(name); builtin != nullptr) {
    return builtin->arity;
  }
  if (const UserFunction* user = environment.find_user_function(name); user != nullptr) {
    return user->params.size();
  }
  return std::nullopt;
}

PlotData sample_plot(const PlotSpec& spec, const Environment& environment) {
  PlotData data;
  data.samples.reserve(kPlotSamples);

  Call invocation;
  invocation.name = spec.name;
  invocation.args.push_back(make_node<Number>(spec.x_min, std::size_t{0}));
  Node call{std::move(invocation)};
  Number& slot = std::get<Number>(std::get<Call>(call.kind).args[0]->kind);

  const Value span = spec.x_max - spec.x_min;
  for (std::size_t index = 0; index < kPlotSamples; ++index) {
    PlotSample sample;
    sample.x = spec.x_min +
               span * static_cast<Value>(index) / static_cast<Value>(kPlotSamples - 1);
    slot.value = sample.x;

    Result<Value> value = evaluate(call, environment);
    if (!value) {
      if (!data.first_error.has_value()) data.first_error = value.error();
    } else if (std::isfinite(value.value())) {
      sample.y = value.value();
      sample.ok = true;
      data.has_points = true;
    }
    data.samples.push_back(sample);
  }

  if (spec.auto_y) {
    bool first = true;
    for (const PlotSample& sample : data.samples) {
      if (!sample.ok) continue;
      if (first || sample.y < data.y_min) data.y_min = sample.y;
      if (first || sample.y > data.y_max) data.y_max = sample.y;
      first = false;
    }
  } else {
    data.y_min = spec.y_min;
    data.y_max = spec.y_max;
  }

  if (!(data.y_max > data.y_min)) {
    const Value middle = data.y_min;
    data.y_min = middle - 1;
    data.y_max = middle + 1;
  }
  return data;
}

int plot_dot_row(const PlotData& data, Value y, int dot_rows) {
  if (dot_rows <= 0) return -1;

  const Value fraction = (data.y_max - y) / (data.y_max - data.y_min);
  const Value row = fraction * static_cast<Value>(dot_rows - 1);

  if (std::isnan(row)) return -1;
  if (row <= -1) return -1;
  if (row >= static_cast<Value>(dot_rows)) return dot_rows;
  return static_cast<int>(std::lround(row));
}

}
