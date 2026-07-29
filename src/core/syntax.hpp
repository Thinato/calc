#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace calc {

// What a run of characters is, for the purpose of colouring it. Only the kinds
// that can be decided by looking at one line in isolation appear here: whether a
// name is *defined* on a line is an evaluation result and arrives separately, on
// LineEval.
enum class SyntaxKind { Comment, Function };

struct SyntaxSpan {
  std::size_t begin = 0;  // byte offsets into the line, like Token::column
  std::size_t end = 0;
  SyntaxKind kind = SyntaxKind::Comment;
};

// The spans of one line worth highlighting, ascending and non-overlapping.
//
// Deliberately not built on tokenize(), for two reasons. tokenize() gives up at
// the first character it cannot read, so `x1 = 5` yields an error and no tokens
// at all — highlighting would vanish on exactly the lines a reader most needs to
// pick apart by eye. And it discards comments entirely, breaking out of its loop
// at '#' without recording where. This pass cannot fail: every line, however
// broken, gets whatever spans it has.
std::vector<SyntaxSpan> syntax_spans(std::string_view line);

}  // namespace calc
