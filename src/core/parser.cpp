#include "core/parser.hpp"

#include <memory>
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

struct ClosureShape {
  std::string name;
  std::size_t arity = 0;
  std::size_t remaining = 0;
};

std::string arity_phrase(const std::string& name, std::size_t arity) {
  return name + "() takes " + std::to_string(arity) +
         (arity == 1 ? " argument" : " arguments");
}

std::string nesting_phrase(std::size_t remaining) {
  if (remaining == 1) return "one more sum";
  return std::to_string(remaining) + " more sums";
}

class Parser {
 public:
  Parser(const std::vector<Token>& tokens, const Environment* environment)
      : tokens_(tokens), environment_(environment) {}

  Result<NodePtr> parse_line() {
    Result<NodePtr> expression = parse_expression(0);
    if (!expression) return expression.error();
    return at_end_of_line(std::move(expression));
  }

  Result<Statement> parse_definition_line() {
    Result<Statement> statement = parse_definition();
    if (!statement) return statement.error();
    return at_end_of_line(std::move(statement));
  }

 private:
  const Token& peek() const { return tokens_[position_]; }
  const Token& advance() { return tokens_[position_++]; }

  Error unexpected() const {
    return make_error(ErrorCode::UnexpectedToken,
                      std::string("unexpected ") + std::string(describe(peek().kind)),
                      peek().column);
  }

  template <typename T>
  Result<T> at_end_of_line(Result<T> parsed) {
    if (peek().kind != TokenKind::End) return unexpected();
    return parsed;
  }

  std::optional<std::size_t> arity_of(const std::string& name) const {
    if (const FunctionDef* builtin = find_function(name); builtin != nullptr) {
      return builtin->arity;
    }
    if (environment_ != nullptr) {
      if (const UserFunction* user = environment_->find_user_function(name);
          user != nullptr) {
        return user->params.size();
      }
    }
    return std::nullopt;
  }

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
    return parse_postfix();
  }

  Result<NodePtr> parse_postfix() {
    Result<NodePtr> value = parse_primary();
    if (!value) return value;

    while (peek().kind == TokenKind::Bang) {
      const std::size_t column = advance().column;
      value = make_node<Factorial>(std::move(value.value()), column);
    }
    return value;
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

    if (token.kind == TokenKind::Sum) {
      Result<NodePtr> sum = parse_sum();
      if (!sum) return sum.error();

      const ClosureShape shape = shape_of(*sum.value());
      if (shape.remaining != 0) {
        return make_error(ErrorCode::SumClosure,
                          arity_phrase(shape.name, shape.arity) + ", so this sum needs " +
                              nesting_phrase(shape.remaining) + " around it",
                          token.column);
      }
      return sum;
    }

    if (token.kind == TokenKind::Identifier) {
      const bool looks_like_call = tokens_[position_ + 1].kind == TokenKind::LParen;
      if (looks_like_call || arity_of(token.text).has_value()) return parse_call();
      advance();
      return make_node<Identifier>(token.text, token.column);
    }

    if (token.kind == TokenKind::End) {
      return make_error(ErrorCode::UnexpectedEnd, "incomplete expression", token.column);
    }
    return unexpected();
  }

  Result<NodePtr> parse_call() {
    const Token name_token = advance();
    const std::optional<std::size_t> arity = arity_of(name_token.text);
    if (!arity) {
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

    if (args.size() != *arity) {
      return make_error(
          ErrorCode::WrongArity,
          arity_phrase(name_token.text, *arity) + ", got " + std::to_string(args.size()),
          name_token.column);
    }
    return make_node<Call>(name_token.text, std::move(args), name_token.column);
  }

  ClosureShape shape_of(const Node& node) const {
    if (const auto* ref = std::get_if<FuncRef>(&node.kind)) {
      const std::size_t arity = arity_of(ref->name).value_or(0);
      return ClosureShape{ref->name, arity, arity};
    }
    ClosureShape shape = shape_of(*std::get<Sum>(node.kind).closure);
    --shape.remaining;
    return shape;
  }

  Error three_arguments(std::size_t column) const {
    return make_error(
        ErrorCode::SumClosure,
        "sum takes three arguments: a first value, a last value, and a function", column);
  }

  Result<NodePtr> parse_closure() {
    if (peek().kind == TokenKind::Sum) return parse_sum();

    if (peek().kind != TokenKind::Identifier) {
      return make_error(ErrorCode::SumClosure, "sum's third argument must be a function",
                        peek().column);
    }
    const Token name = advance();
    if (peek().kind == TokenKind::LParen) {
      return make_error(ErrorCode::SumClosure,
                        "sum's third argument must be a function, not a call",
                        name.column);
    }
    if (!arity_of(name.text).has_value()) {
      return make_error(ErrorCode::UnknownFunction,
                        "unknown function '" + name.text + "'", name.column);
    }
    return make_node<FuncRef>(name.text, name.column);
  }

  Result<NodePtr> parse_sum() {
    const Token keyword = advance();
    if (peek().kind != TokenKind::LParen) {
      return make_error(ErrorCode::ExpectedCallParen, "expected '(' after 'sum'",
                        peek().column);
    }
    const std::size_t open_column = advance().column;

    Sum sum;
    sum.column = keyword.column;

    Result<NodePtr> first = parse_expression(0);
    if (!first) return first.error();
    sum.first = std::move(first.value());
    if (peek().kind != TokenKind::Comma) return three_arguments(keyword.column);
    advance();

    Result<NodePtr> last = parse_expression(0);
    if (!last) return last.error();
    sum.last = std::move(last.value());
    if (peek().kind != TokenKind::Comma) return three_arguments(keyword.column);
    advance();

    Result<NodePtr> closure = parse_closure();
    if (!closure) return closure.error();

    const ClosureShape shape = shape_of(*closure.value());
    if (shape.remaining == 0) {
      if (std::holds_alternative<Sum>(closure.value()->kind)) {
        return make_error(ErrorCode::SumClosure,
                          "a sum over " + shape.name + "() is already a number",
                          keyword.column);
      }
      return make_error(
          ErrorCode::SumClosure,
          shape.name + "() takes no arguments, so there is nothing to sum over",
          keyword.column);
    }
    sum.closure = std::move(closure.value());

    if (peek().kind == TokenKind::Comma) return three_arguments(keyword.column);
    if (peek().kind != TokenKind::RParen) {
      return make_error(ErrorCode::UnbalancedParen, "unclosed '('", open_column);
    }
    advance();
    return std::make_unique<Node>(Node{std::move(sum)});
  }

  Result<Statement> parse_definition() {
    advance();

    if (peek().kind != TokenKind::Identifier) {
      return make_error(ErrorCode::DefineName, "expected a name after 'define'",
                        peek().column);
    }
    const Token name = advance();

    auto declaration = std::make_unique<FunctionDecl>();
    declaration->name = name.text;
    declaration->name_column = name.column;

    if (peek().kind != TokenKind::LParen) {
      return make_error(ErrorCode::DefineName, "expected '(' after '" + name.text + "'",
                        peek().column);
    }
    const std::size_t open_paren = advance().column;

    if (peek().kind != TokenKind::RParen) {
      while (true) {
        if (peek().kind != TokenKind::Identifier) {
          return make_error(ErrorCode::DefineName, "expected a parameter name",
                            peek().column);
        }
        const Token param = advance();
        for (const Param& seen : declaration->params) {
          if (seen.name == param.text) {
            return make_error(ErrorCode::DuplicateParameter,
                              "duplicate parameter '" + param.text + "'", param.column);
          }
        }
        declaration->params.push_back(Param{param.text, param.column});
        if (peek().kind != TokenKind::Comma) break;
        advance();
      }
    }
    if (peek().kind != TokenKind::RParen) {
      return make_error(ErrorCode::UnbalancedParen, "unclosed '('", open_paren);
    }
    advance();

    if (peek().kind == TokenKind::Colon) {
      advance();
      Result<std::vector<Statement>> body = parse_body(TokenKind::End, 0);
      if (!body) return body.error();
      declaration->body = std::move(body.value());
    } else if (peek().kind == TokenKind::LBrace) {
      const std::size_t open_brace = advance().column;
      Result<std::vector<Statement>> body = parse_body(TokenKind::RBrace, open_brace);
      if (!body) return body.error();
      advance();
      declaration->body = std::move(body.value());
    } else {
      return make_error(ErrorCode::DefineName, "expected ':' or '{' after the parameters",
                        peek().column);
    }

    Statement statement;
    statement.definition = std::move(declaration);
    return statement;
  }

  bool unclosed(TokenKind terminator) const {
    return terminator == TokenKind::RBrace && peek().kind == TokenKind::End;
  }

  Result<std::vector<Statement>> parse_body(TokenKind terminator,
                                            std::size_t open_brace) {
    std::vector<Statement> statements;
    bool returned = false;

    while (true) {
      while (peek().kind == TokenKind::Semicolon) advance();
      if (peek().kind == terminator) break;

      if (unclosed(terminator)) {
        return make_error(ErrorCode::UnbalancedParen, "unclosed '{'", open_brace);
      }

      if (returned) {
        return make_error(ErrorCode::ReturnNotLast, "'return' must be the last statement",
                          peek().column);
      }
      if (peek().kind == TokenKind::Define) {
        return make_error(ErrorCode::UnexpectedToken,
                          "a definition cannot contain a definition", peek().column);
      }
      if (peek().kind == TokenKind::Return) {
        advance();
        returned = true;
      }

      Result<Statement> statement = parse_body_statement();
      if (!statement) return statement.error();
      statements.push_back(std::move(statement.value()));

      if (peek().kind == TokenKind::Semicolon) continue;
      if (peek().kind == terminator) break;
      if (unclosed(terminator)) {
        return make_error(ErrorCode::UnbalancedParen, "unclosed '{'", open_brace);
      }
      return unexpected();
    }

    if (statements.empty()) {
      return make_error(ErrorCode::EmptyBody, "a body needs an expression",
                        peek().column);
    }
    return statements;
  }

  Result<Statement> parse_body_statement() {
    Statement statement;
    if (peek().kind == TokenKind::Identifier &&
        tokens_[position_ + 1].kind == TokenKind::Equals) {
      const Token name = advance();
      advance();
      statement.target = name.text;
      statement.target_column = name.column;
    }

    Result<NodePtr> expression = parse_expression(0);
    if (!expression) return expression.error();
    statement.expression = std::move(expression.value());
    return statement;
  }

  const std::vector<Token>& tokens_;
  const Environment* environment_ = nullptr;
  std::size_t position_ = 0;
};

}

Result<NodePtr> parse(const std::vector<Token>& tokens, const Environment* environment) {
  return Parser(tokens, environment).parse_line();
}

Result<NodePtr> parse(std::string_view line, const Environment* environment) {
  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) return tokens.error();
  return parse(tokens.value(), environment);
}

Result<Statement> parse_statement(const std::vector<Token>& tokens,
                                  const Environment* environment) {
  if (tokens[0].kind == TokenKind::Define) {
    return Parser(tokens, environment).parse_definition_line();
  }
  if (tokens[0].kind == TokenKind::Return) {
    return make_error(ErrorCode::ReturnOutsideBody,
                      "'return' only means something inside a { } body",
                      tokens[0].column);
  }

  constexpr std::size_t kNone = static_cast<std::size_t>(-1);
  std::size_t equals = kNone;
  std::size_t second_equals = kNone;
  int depth = 0;

  for (std::size_t index = 0; index < tokens.size(); ++index) {
    switch (tokens[index].kind) {
      case TokenKind::LParen:
      case TokenKind::LBrace: ++depth; break;
      case TokenKind::RParen:
      case TokenKind::RBrace: --depth; break;
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
    Result<NodePtr> expression = parse(tokens, environment);
    if (!expression) return expression.error();
    return Statement{std::nullopt, 0, std::move(expression.value()), nullptr};
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
  Result<NodePtr> expression = parse(value, environment);
  if (!expression) return expression.error();

  return Statement{tokens[0].text, tokens[0].column, std::move(expression.value()),
                   nullptr};
}

Result<Statement> parse_statement(std::string_view line, const Environment* environment) {
  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) return tokens.error();
  return parse_statement(tokens.value(), environment);
}

}
