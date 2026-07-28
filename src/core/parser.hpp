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
//   primary := number | '(' expr ')' | name '(' expr (',' expr)* ')'
Result<NodePtr> parse(const std::vector<Token>& tokens);

// Convenience: tokenize then parse.
Result<NodePtr> parse(std::string_view line);

}  // namespace calc
