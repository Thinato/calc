#include "core/syntax.hpp"

#include "core/functions.hpp"
#include "core/lexer.hpp"

namespace calc {

std::vector<SyntaxSpan> syntax_spans(std::string_view line) {
  std::vector<SyntaxSpan> spans;

  std::size_t index = 0;
  while (index < line.size()) {
    // A comment swallows the rest of the line, so nothing inside it is looked at
    // again: "# sqrt(16)" is prose that happens to mention a function, not a
    // call. The language has no string literals, which is what makes the first
    // '#' unconditionally the start of one.
    if (line[index] == '#') {
      spans.push_back(SyntaxSpan{index, line.size(), SyntaxKind::Comment});
      break;
    }

    if (is_ident_start(line[index])) {
      const std::size_t start = index;
      while (index < line.size() && is_ident_continue(line[index])) ++index;
      // Taking the whole run before deciding is what keeps "sqrtx" plain instead
      // of colouring its first four characters.
      if (find_function(line.substr(start, index - start)) != nullptr) {
        spans.push_back(SyntaxSpan{start, index, SyntaxKind::Function});
      }
      continue;
    }

    // Anything else is punctuation, a number, or a byte of a multi-byte
    // character — none of which is highlighted. UTF-8 continuation bytes are all
    // >= 0x80, so they can never be mistaken for '#' or for an ASCII letter, and
    // stepping a byte at a time is safe here.
    ++index;
  }

  return spans;
}

}  // namespace calc
