#pragma once

#include <cstddef>

#include <ftxui/dom/elements.hpp>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/engine.hpp"

namespace calc {

// The visible window onto the buffer. It lives in the UI layer because only the
// renderer knows how tall the terminal is; the document knows nothing about the
// screen.
struct Viewport {
  std::size_t top_row = 0;
  std::size_t height = 20;
};

// Rows of chrome below the text area: separator, status line, message line.
constexpr std::size_t kChromeRows = 3;

// Scrolls the buffer so the cursor stays visible with a few lines of context.
void follow_cursor(Viewport& viewport, const Document& document);

// Applies a scroll the engine asked for but could not perform itself. Takes the
// document by reference because Ctrl-D and Ctrl-U move the cursor, not the view.
void apply_scroll(Viewport& viewport, ScrollRequest request, Document& document);

// Builds one frame. Pure in its inputs, which is what makes the layout
// snapshot-testable against a plain ftxui::Screen with no terminal attached.
ftxui::Element render_frame(const Document& document, const ResultCache& results,
                            const VimEngine& engine, const Viewport& viewport);

}  // namespace calc
