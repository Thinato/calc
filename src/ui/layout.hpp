#pragma once

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/engine.hpp"

namespace calc {

struct Viewport {
  std::size_t top_row = 0;
  std::size_t height = 20;
};

constexpr std::size_t kChromeRows = 3;

void follow_cursor(Viewport& viewport, const Document& document);

void apply_scroll(Viewport& viewport, ScrollRequest request, Document& document);

ftxui::Element render_frame(const Document& document, const ResultCache& results,
                            const VimEngine& engine, const Viewport& viewport);

}
