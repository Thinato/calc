#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/value.hpp"

namespace calc {

enum class TokenKind {
  Number,
  Plus,
  Minus,
  Star,
  Slash,
  Caret,
  LParen,
  RParen,
  Comma,
  Identifier,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::size_t column = 0;  // 0-based byte offset of the token's first character
  Value number = 0;        // only meaningful for TokenKind::Number
  std::string text;        // only meaningful for TokenKind::Identifier
};

// Splits a line into tokens. Whitespace is skipped and a '#' starts a comment
// that runs to the end of the line. The returned vector always ends with an
// End token.
Result<std::vector<Token>> tokenize(std::string_view line);

// True when the line holds nothing to evaluate: empty, whitespace only, or a
// comment only.
bool is_blank_or_comment(std::string_view line);

// Human-readable token name, for error messages.
std::string_view describe(TokenKind kind);

}  // namespace calc
