#include "core/lexer.hpp"

#include <cctype>
#include <cstdlib>

namespace calc {
namespace {

bool is_space(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

// Builds the diagnostic for a name that cannot exist, quoting the whole span the
// user typed so the message names the thing they wrote.
Error invalid_name(std::string_view span, std::size_t column, std::string_view reason) {
  return make_error(
      ErrorCode::InvalidName,
      "invalid name '" + std::string(span) + "': names cannot " + std::string(reason),
      column);
}

}  // namespace

bool is_ident_start(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool is_ident_continue(char c) { return is_ident_start(c) || is_digit(c); }

std::string_view describe(TokenKind kind) {
  switch (kind) {
    case TokenKind::Number: return "a number";
    case TokenKind::Plus: return "'+'";
    case TokenKind::Minus: return "'-'";
    case TokenKind::Star: return "'*'";
    case TokenKind::Slash: return "'/'";
    case TokenKind::Caret: return "'^'";
    case TokenKind::LParen: return "'('";
    case TokenKind::RParen: return "')'";
    case TokenKind::Comma: return "','";
    case TokenKind::Equals: return "'='";
    case TokenKind::Identifier: return "a name";
    case TokenKind::End: return "end of line";
  }
  return "something";
}

bool is_blank_or_comment(std::string_view line) {
  for (char c : line) {
    if (is_space(c)) continue;
    return c == '#';
  }
  return true;
}

Result<std::vector<Token>> tokenize(std::string_view line) {
  std::vector<Token> tokens;
  std::size_t i = 0;

  while (i < line.size()) {
    const char c = line[i];

    if (is_space(c)) {
      ++i;
      continue;
    }
    if (c == '#') break;  // comment to end of line

    Token token;
    token.column = i;

    // A number: digits with an optional fraction and an optional exponent.
    // Exponents are accepted so that a result this program printed (1e+30) can
    // be pasted back in and re-read.
    if (is_digit(c) || (c == '.' && i + 1 < line.size() && is_digit(line[i + 1]))) {
      const std::size_t start = i;
      while (i < line.size() && is_digit(line[i])) ++i;
      if (i < line.size() && line[i] == '.') {
        ++i;
        while (i < line.size() && is_digit(line[i])) ++i;
      }
      if (i < line.size() && (line[i] == 'e' || line[i] == 'E')) {
        const std::size_t exponent_start = i;
        ++i;
        if (i < line.size() && (line[i] == '+' || line[i] == '-')) ++i;
        if (i < line.size() && is_digit(line[i])) {
          while (i < line.size() && is_digit(line[i])) ++i;
        } else {
          i = exponent_start;  // not an exponent after all, e.g. "2e" or "3end"
        }
      }

      // A digit run touching a letter or underscore is a name that starts with a
      // digit, which no name may do. Reported over the whole span so "1_x" is
      // named rather than dissolving into a number and a separate name.
      if (i < line.size() && is_ident_start(line[i])) {
        std::size_t end = i;
        while (end < line.size() && is_ident_continue(line[end])) ++end;
        return invalid_name(line.substr(start, end - start), start, "start with a digit");
      }

      const std::string digits(line.substr(start, i - start));
      token.kind = TokenKind::Number;
      token.number = std::strtod(digits.c_str(), nullptr);
      tokens.push_back(std::move(token));
      continue;
    }

    if (is_ident_start(c)) {
      const std::size_t start = i;
      while (i < line.size() && is_ident_continue(line[i])) ++i;
      const std::string_view span = line.substr(start, i - start);

      // A '.' touching a name is part of the name the user meant, not an
      // operator: "x.y" is one bad name rather than two good ones.
      if (i < line.size() && line[i] == '.') {
        std::size_t end = i + 1;
        while (end < line.size() && is_ident_continue(line[end])) ++end;
        return invalid_name(line.substr(start, end - start), start, "contain '.'");
      }
      if (span.find_first_of("0123456789") != std::string_view::npos) {
        return invalid_name(span, start, "contain digits");
      }

      token.kind = TokenKind::Identifier;
      token.text = std::string(span);
      tokens.push_back(std::move(token));
      continue;
    }

    switch (c) {
      case '+': token.kind = TokenKind::Plus; break;
      case '-': token.kind = TokenKind::Minus; break;
      case '*': token.kind = TokenKind::Star; break;
      case '/': token.kind = TokenKind::Slash; break;
      case '^': token.kind = TokenKind::Caret; break;
      case '(': token.kind = TokenKind::LParen; break;
      case ')': token.kind = TokenKind::RParen; break;
      case ',': token.kind = TokenKind::Comma; break;
      case '=': token.kind = TokenKind::Equals; break;
      default:
        return make_error(ErrorCode::UnexpectedCharacter,
                          std::string("unexpected character '") + c + "'", i);
    }
    ++i;
    tokens.push_back(std::move(token));
  }

  Token end;
  end.kind = TokenKind::End;
  end.column = line.size();
  tokens.push_back(std::move(end));
  return tokens;
}

}  // namespace calc
