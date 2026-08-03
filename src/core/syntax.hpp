#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace calc {

enum class SyntaxKind { Comment, Function };

struct SyntaxSpan {
  std::size_t begin = 0;
  std::size_t end = 0;
  SyntaxKind kind = SyntaxKind::Comment;
};

std::vector<SyntaxSpan> syntax_spans(std::string_view line);

}
