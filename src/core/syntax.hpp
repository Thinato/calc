#pragma once

#include <cstddef>
#include <functional>
#include <string_view>
#include <vector>

namespace calc {

enum class SyntaxKind { Comment, Function, Keyword };

struct SyntaxSpan {
  std::size_t begin = 0;
  std::size_t end = 0;
  SyntaxKind kind = SyntaxKind::Comment;
};

using FunctionLookup = std::function<bool(std::string_view)>;

std::vector<SyntaxSpan> syntax_spans(std::string_view line,
                                     const FunctionLookup& known = {});

}
