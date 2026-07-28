#include "core/eval.hpp"

#include <cmath>
#include <vector>

#include "core/functions.hpp"

namespace calc {
namespace {

// Every arithmetic step funnels through here so an overflow is reported where
// it happened rather than surfacing as "inf" in the result column.
Result<Value> checked(Value value, std::size_t column) {
  if (std::isinf(value)) {
    return make_error(ErrorCode::NotFinite, "result is too large", column);
  }
  if (std::isnan(value)) {
    return make_error(ErrorCode::DomainError, "result is undefined", column);
  }
  return value;
}

Result<Value> evaluate_binary(const Binary& node) {
  Result<Value> lhs = evaluate(*node.lhs);
  if (!lhs) return lhs.error();
  Result<Value> rhs = evaluate(*node.rhs);
  if (!rhs) return rhs.error();

  const Value a = lhs.value();
  const Value b = rhs.value();

  switch (node.op) {
    case '+': return checked(a + b, node.column);
    case '-': return checked(a - b, node.column);
    case '*': return checked(a * b, node.column);
    case '/':
      if (b == 0) {
        return make_error(ErrorCode::DivisionByZero, "division by zero", node.column);
      }
      return checked(a / b, node.column);
    case '^': {
      const Value result = std::pow(a, b);
      if (std::isnan(result) && !std::isnan(a) && !std::isnan(b)) {
        return make_error(ErrorCode::DomainError, "'^' is undefined here", node.column);
      }
      return checked(result, node.column);
    }
    default: break;
  }
  return make_error(ErrorCode::UnexpectedToken, "unsupported operator", node.column);
}

Result<Value> evaluate_call(const Call& node) {
  const FunctionDef* definition = find_function(node.name);
  if (definition == nullptr) {
    return make_error(ErrorCode::UnknownFunction,
                      "unknown function '" + node.name + "'", node.column);
  }

  std::vector<Value> args;
  args.reserve(node.args.size());
  for (const NodePtr& argument : node.args) {
    Result<Value> value = evaluate(*argument);
    if (!value) return value.error();
    args.push_back(value.value());
  }

  Result<Value> result = definition->apply(args, node.column);
  if (!result) return result.error();
  return checked(result.value(), node.column);
}

}  // namespace

Result<Value> evaluate(const Node& node) {
  if (const auto* number = std::get_if<Number>(&node.kind)) {
    return number->value;
  }
  if (const auto* unary = std::get_if<Unary>(&node.kind)) {
    Result<Value> operand = evaluate(*unary->operand);
    if (!operand) return operand.error();
    return unary->op == '-' ? -operand.value() : operand.value();
  }
  if (const auto* binary = std::get_if<Binary>(&node.kind)) {
    return evaluate_binary(*binary);
  }
  return evaluate_call(std::get<Call>(node.kind));
}

}  // namespace calc
