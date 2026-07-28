#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "core/value.hpp"

namespace calc {

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Number {
  Value value = 0;
  std::size_t column = 0;
};

// A reference to a variable or constant, resolved against the Environment at
// evaluation time.
struct Identifier {
  std::string name;
  std::size_t column = 0;
};

struct Unary {
  char op = '-';  // '-' or '+'
  NodePtr operand;
  std::size_t column = 0;
};

struct Binary {
  char op = '+';  // '+' '-' '*' '/' '^'
  NodePtr lhs;
  NodePtr rhs;
  std::size_t column = 0;  // column of the operator itself
};

struct Call {
  std::string name;
  std::vector<NodePtr> args;
  std::size_t column = 0;
};

struct Node {
  std::variant<Number, Identifier, Unary, Binary, Call> kind;
};

// One line of the buffer: either a bare expression, or `name = expression`.
struct Statement {
  std::optional<std::string> target;  // empty for a bare expression
  std::size_t target_column = 0;
  NodePtr expression;
};

// True for a value the user wrote out directly, so the renderer can skip
// restating a number that is already on screen. A sign still counts as literal,
// and so do redundant parentheses — the parser folds those away, and `x = (5)`
// gains nothing from a `= 5` either.
inline bool is_literal(const Node& node) {
  if (std::holds_alternative<Number>(node.kind)) return true;
  if (const auto* unary = std::get_if<Unary>(&node.kind)) {
    return std::holds_alternative<Number>(unary->operand->kind);
  }
  return false;
}

template <typename T, typename... Args>
NodePtr make_node(Args&&... args) {
  return std::make_unique<Node>(Node{T{std::forward<Args>(args)...}});
}

}  // namespace calc
