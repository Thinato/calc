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
  LBrace,
  RBrace,
  Comma,
  Colon,
  Semicolon,
  Equals,
  Identifier,
  Define,
  Return,
  Sum,
  End,
};

struct Token {
  TokenKind kind = TokenKind::End;
  std::size_t column = 0;
  Value number = 0;
  std::string text;
};

Result<std::vector<Token>> tokenize(std::string_view line);

bool is_ident_start(char c);
bool is_ident_continue(char c);

bool is_blank_or_comment(std::string_view line);

std::string_view describe(TokenKind kind);

}
