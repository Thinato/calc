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

struct Identifier {
  std::string name;
  std::size_t column = 0;
};

struct Unary {
  char op = '-';
  NodePtr operand;
  std::size_t column = 0;
};

struct Binary {
  char op = '+';
  NodePtr lhs;
  NodePtr rhs;
  std::size_t column = 0;
};

struct Call {
  std::string name;
  std::vector<NodePtr> args;
  std::size_t column = 0;
};

struct Node {
  std::variant<Number, Identifier, Unary, Binary, Call> kind;
};

struct Statement {
  std::optional<std::string> target;
  std::size_t target_column = 0;
  NodePtr expression;
};

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

}
