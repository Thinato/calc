#include "core/parser.hpp"

#include <optional>
#include <string>
#include <utility>

#include "core/functions.hpp"

namespace calc {
namespace {

constexpr int kPrecAdditive = 10;
constexpr int kPrecMultiplicative = 20;
constexpr int kPrecUnary = 25;
constexpr int kPrecPower = 30;

struct BinaryInfo {
  char op;
  int precedence;
  bool right_associative;
};

std::optional<BinaryInfo> binary_info(TokenKind kind) {
  switch (kind) {
    case TokenKind::Plus: return BinaryInfo{'+', kPrecAdditive, false};
    case TokenKind::Minus: return BinaryInfo{'-', kPrecAdditive, false};
    case TokenKind::Star: return BinaryInfo{'*', kPrecMultiplicative, false};
    case TokenKind::Slash: return BinaryInfo{'/', kPrecMultiplicative, false};
    case TokenKind::Caret: return BinaryInfo{'^', kPrecPower, true};
    default: return std::nullopt;
  }
}

class Parser {
 public:
  explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

  Result<NodePtr> parse_line() {
    Result<NodePtr> expression = parse_expression(0);
    if (!expression) return expression.error();
    if (peek().kind != TokenKind::End) {
      return make_error(ErrorCode::UnexpectedToken,
                        std::string("unexpected ") + std::string(describe(peek().kind)),
                        peek().column);
    }
    return expression;
  }

 private:
  const Token& peek() const { return tokens_[position_]; }
  const Token& advance() { return tokens_[position_++]; }

  Result<NodePtr> parse_expression(int min_precedence) {
    Result<NodePtr> lhs = parse_prefix();
    if (!lhs) return lhs.error();

    while (true) {
      const std::optional<BinaryInfo> info = binary_info(peek().kind);
      if (!info || info->precedence < min_precedence) break;

      const std::size_t column = advance().column;
      const int next_min =
          info->right_associative ? info->precedence : info->precedence + 1;

      Result<NodePtr> rhs = parse_expression(next_min);
      if (!rhs) return rhs.error();

      lhs = make_node<Binary>(info->op, std::move(lhs.value()), std::move(rhs.value()),
                              column);
    }
    return lhs;
  }

  Result<NodePtr> parse_prefix() {
    if (peek().kind == TokenKind::Minus || peek().kind == TokenKind::Plus) {
      const Token& token = advance();
      const char op = token.kind == TokenKind::Minus ? '-' : '+';
      Result<NodePtr> operand = parse_expression(kPrecUnary);
      if (!operand) return operand.error();
      return make_node<Unary>(op, std::move(operand.value()), token.column);
    }
    return parse_primary();
  }

  Result<NodePtr> parse_primary() {
    const Token& token = peek();

    if (token.kind == TokenKind::Number) {
      advance();
      return make_node<Number>(token.number, token.column);
    }

    if (token.kind == TokenKind::LParen) {
      const std::size_t open_column = advance().column;
      Result<NodePtr> inner = parse_expression(0);
      if (!inner) return inner.error();
      if (peek().kind != TokenKind::RParen) {
        return make_error(ErrorCode::UnbalancedParen, "unclosed '('", open_column);
      }
      advance();
      return inner;
    }

    if (token.kind == TokenKind::Identifier) {
      const bool looks_like_call = tokens_[position_ + 1].kind == TokenKind::LParen;
      if (looks_like_call || find_function(token.text) != nullptr) return parse_call();
      advance();
      return make_node<Identifier>(token.text, token.column);
    }

    if (token.kind == TokenKind::End) {
      return make_error(ErrorCode::UnexpectedEnd, "incomplete expression", token.column);
    }
    return make_error(ErrorCode::UnexpectedToken,
                      std::string("unexpected ") + std::string(describe(token.kind)),
                      token.column);
  }

  Result<NodePtr> parse_call() {
    const Token name_token = advance();
    const FunctionDef* definition = find_function(name_token.text);
    if (definition == nullptr) {
      return make_error(ErrorCode::UnknownFunction,
                        "unknown function '" + name_token.text + "'", name_token.column);
    }
    if (peek().kind != TokenKind::LParen) {
      return make_error(ErrorCode::ExpectedCallParen,
                        "expected '(' after '" + name_token.text + "'", peek().column);
    }
    const std::size_t open_column = advance().column;

    std::vector<NodePtr> args;
    if (peek().kind != TokenKind::RParen) {
      while (true) {
        Result<NodePtr> argument = parse_expression(0);
        if (!argument) return argument.error();
        args.push_back(std::move(argument.value()));
        if (peek().kind != TokenKind::Comma) break;
        advance();
      }
    }
    if (peek().kind != TokenKind::RParen) {
      return make_error(ErrorCode::UnbalancedParen, "unclosed '('", open_column);
    }
    advance();

    if (args.size() != definition->arity) {
      return make_error(
          ErrorCode::WrongArity,
          name_token.text + "() takes " + std::to_string(definition->arity) +
              (definition->arity == 1 ? " argument, got " : " arguments, got ") +
              std::to_string(args.size()),
          name_token.column);
    }
    return make_node<Call>(name_token.text, std::move(args), name_token.column);
  }

  const std::vector<Token>& tokens_;
  std::size_t position_ = 0;
};

}

Result<NodePtr> parse(const std::vector<Token>& tokens) {
  return Parser(tokens).parse_line();
}

Result<NodePtr> parse(std::string_view line) {
  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) return tokens.error();
  return parse(tokens.value());
}

Result<Statement> parse_statement(const std::vector<Token>& tokens) {
  constexpr std::size_t kNone = static_cast<std::size_t>(-1);
  std::size_t equals = kNone;
  std::size_t second_equals = kNone;
  int depth = 0;

  for (std::size_t index = 0; index < tokens.size(); ++index) {
    switch (tokens[index].kind) {
      case TokenKind::LParen: ++depth; break;
      case TokenKind::RParen: --depth; break;
      case TokenKind::Equals:
        if (depth == 0) {
          if (equals == kNone) {
            equals = index;
          } else if (second_equals == kNone) {
            second_equals = index;
          }
        }
        break;
      default: break;
    }
  }

  if (equals == kNone) {
    Result<NodePtr> expression = parse(tokens);
    if (!expression) return expression.error();
    return Statement{std::nullopt, 0, std::move(expression.value())};
  }

  if (second_equals != kNone) {
    return make_error(ErrorCode::MultipleAssignment, "only one '=' per line",
                      tokens[second_equals].column);
  }
  if (equals == 0) {
    return make_error(ErrorCode::AssignmentTarget, "expected a name before '='",
                      tokens[0].column);
  }
  if (equals != 1 || tokens[0].kind != TokenKind::Identifier) {
    return make_error(ErrorCode::AssignmentTarget, "the left of '=' must be a name",
                      tokens[equals].column);
  }

  const std::vector<Token> value(tokens.begin() + static_cast<std::ptrdiff_t>(equals) + 1,
                                 tokens.end());
  Result<NodePtr> expression = parse(value);
  if (!expression) return expression.error();

  return Statement{tokens[0].text, tokens[0].column, std::move(expression.value())};
}

Result<Statement> parse_statement(std::string_view line) {
  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) return tokens.error();
  return parse_statement(tokens.value());
}

}
