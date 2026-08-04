#pragma once

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "ui/plot_view.hpp"
#include "vim/engine.hpp"

namespace calc {

struct Viewport {
  std::size_t top_row = 0;
  std::size_t height = 20;
  std::size_t plot_height = 0;
};

constexpr std::size_t kChromeRows = 3;

constexpr std::size_t kPlotMinRows = 8;

constexpr std::size_t kPlotBufferFloor = 3;

std::size_t plot_rows(std::size_t screen_rows, bool plot_open);

void follow_cursor(Viewport& viewport, const Document& document);

void apply_scroll(Viewport& viewport, ScrollRequest request, Document& document);

ftxui::Element render_frame(const Document& document, const ResultCache& results,
                            const VimEngine& engine, const Viewport& viewport);

}
