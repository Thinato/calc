#include "core/syntax.hpp"

#include "core/functions.hpp"
#include "core/lexer.hpp"

namespace calc {

std::vector<SyntaxSpan> syntax_spans(std::string_view line, const FunctionLookup& known) {
  std::vector<SyntaxSpan> spans;

  std::size_t index = 0;
  while (index < line.size()) {
    if (line[index] == '#') {
      spans.push_back(SyntaxSpan{index, line.size(), SyntaxKind::Comment});
      break;
    }

    if (is_ident_start(line[index])) {
      const std::size_t start = index;
      while (index < line.size() && is_ident_continue(line[index])) ++index;
      const std::string_view word = line.substr(start, index - start);

      if (word == "define" || word == "return") {
        spans.push_back(SyntaxSpan{start, index, SyntaxKind::Keyword});
      } else if (find_function(word) != nullptr || (known && known(word))) {
        spans.push_back(SyntaxSpan{start, index, SyntaxKind::Function});
      }
      continue;
    }

    ++index;
  }

  return spans;
}

}
