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
  Equals,
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
//
// Names are letters and underscores only. Identifiers are still read greedily
// including digits so that a malformed name arrives here whole and can be
// reported as such: "x1" produces one InvalidName error rather than silently
// splitting into the name "x" and the number "1".
Result<std::vector<Token>> tokenize(std::string_view line);

// The character rules a name obeys, exported so that anything else needing to
// agree with the lexer about where a name begins and ends can ask rather than
// keep its own copy. Letters and underscores start one; a digit may only
// continue one, which is what lets "x1" be read whole and rejected as a name
// instead of splitting into "x" and "1".
bool is_ident_start(char c);
bool is_ident_continue(char c);

// True when the line holds nothing to evaluate: empty, whitespace only, or a
// comment only.
bool is_blank_or_comment(std::string_view line);

// Human-readable token name, for error messages.
std::string_view describe(TokenKind kind);

}  // namespace calc
