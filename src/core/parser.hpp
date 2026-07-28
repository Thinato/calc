#pragma once

#include <string_view>
#include <vector>

#include "core/ast.hpp"
#include "core/lexer.hpp"
#include "core/result.hpp"

namespace calc {

// Precedence-climbing parser.
//
//   expr    := unary (binop unary)*
//   binop   := '+' '-'  prec 10, left
//            | '*' '/'  prec 20, left
//            | '^'      prec 30, right
//   unary   := ('-' | '+')* primary   -- operand parsed at prec 25, so unary
//                                        binds looser than '^': -2^2 == -4
//   primary := number | name | '(' expr ')' | name '(' expr (',' expr)* ')'
Result<NodePtr> parse(const std::vector<Token>& tokens);

// Convenience: tokenize then parse.
Result<NodePtr> parse(std::string_view line);

// Parses a whole line, which may be an assignment:
//
//   statement := [ name '=' ] expr
//
// The '=' is located at paren depth zero, so anything other than a single name
// on its left is reported as a bad assignment target rather than as a stray
// token. That is what turns `(x) = 5` into an explainable error.
Result<Statement> parse_statement(const std::vector<Token>& tokens);
Result<Statement> parse_statement(std::string_view line);

}  // namespace calc
