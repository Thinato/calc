#pragma once

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "core/environment.hpp"
#include "core/plot.hpp"

namespace calc {

constexpr std::size_t kPlotPanelChrome = 3;

ftxui::Element plot_panel(const PlotSpec& spec, const Environment& environment,
                          std::size_t rows);

}
