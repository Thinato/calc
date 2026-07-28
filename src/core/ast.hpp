#pragma once

#include <cstddef>
#include <memory>
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
  std::variant<Number, Unary, Binary, Call> kind;
};

template <typename T, typename... Args>
NodePtr make_node(Args&&... args) {
  return std::make_unique<Node>(Node{T{std::forward<Args>(args)...}});
}

}  // namespace calc
