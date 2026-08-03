#include "core/eval.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "core/functions.hpp"

namespace calc {
namespace {

constexpr std::size_t kMaxCallDepth = 64;

Result<Value> checked(Value value, std::size_t column) {
  if (std::isinf(value)) {
    return make_error(ErrorCode::NotFinite, "result is too large", column);
  }
  if (std::isnan(value)) {
    return make_error(ErrorCode::DomainError, "result is undefined", column);
  }
  return value;
}

Result<Value> evaluate_binary(const Binary& node, const Environment& environment) {
  Result<Value> lhs = evaluate(*node.lhs, environment);
  if (!lhs) return lhs.error();
  Result<Value> rhs = evaluate(*node.rhs, environment);
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

Error from_body(const Error& error, const Call& node) {
  Error wrapped = error;
  if (!wrapped.in_body) {
    wrapped.message = "in " + node.name + "(): " + wrapped.message;
    wrapped.in_body = true;
  }
  wrapped.column = node.column;
  return wrapped;
}

Result<Value> evaluate_user_call(const UserFunction& function, const Call& node,
                                 const std::vector<Value>& args,
                                 const Environment& environment) {
  if (environment.depth() >= kMaxCallDepth) {
    Error error =
        make_error(ErrorCode::TooMuchRecursion,
                   "too much recursion in '" + function.name + "'", node.column);
    error.in_body = true;
    return error;
  }

  Environment scope = environment.child_for_call();
  for (std::size_t index = 0; index < function.params.size(); ++index) {
    const Param& param = function.params[index];
    if (std::optional<Error> failed =
            scope.define(param.name, args[index], function.defined_row, param.column)) {
      return from_body(*failed, node);
    }
  }

  Value last = 0;
  for (const Statement& statement : function.body) {
    Result<Value> value = evaluate(*statement.expression, scope);
    if (!value) return from_body(value.error(), node);
    if (statement.target.has_value()) {
      if (std::optional<Error> failed =
              scope.define(*statement.target, value.value(), function.defined_row,
                           statement.target_column)) {
        return from_body(*failed, node);
      }
    }
    last = value.value();
  }
  return last;
}

Result<Value> evaluate_call(const Call& node, const Environment& environment) {
  const FunctionDef* builtin = find_function(node.name);
  const UserFunction* user =
      builtin == nullptr ? environment.find_user_function(node.name) : nullptr;
  if (builtin == nullptr && user == nullptr) {
    return make_error(ErrorCode::UnknownFunction, "unknown function '" + node.name + "'",
                      node.column);
  }

  const std::size_t arity = builtin != nullptr ? builtin->arity : user->params.size();
  if (node.args.size() != arity) {
    return make_error(ErrorCode::WrongArity,
                      node.name + "() takes " + std::to_string(arity) +
                          (arity == 1 ? " argument, got " : " arguments, got ") +
                          std::to_string(node.args.size()),
                      node.column);
  }

  std::vector<Value> args;
  args.reserve(node.args.size());
  for (const NodePtr& argument : node.args) {
    Result<Value> value = evaluate(*argument, environment);
    if (!value) return value.error();
    args.push_back(value.value());
  }

  Result<Value> result = user != nullptr
                             ? evaluate_user_call(*user, node, args, environment)
                             : builtin->apply(args, node.column);
  if (!result) return result.error();
  return checked(result.value(), node.column);
}

}

Result<Value> evaluate(const Node& node, const Environment& environment) {
  if (const auto* number = std::get_if<Number>(&node.kind)) {
    return number->value;
  }
  if (const auto* identifier = std::get_if<Identifier>(&node.kind)) {
    const Binding* binding = environment.find(identifier->name);
    if (binding == nullptr) {
      return make_error(ErrorCode::UndefinedName,
                        "undefined name '" + identifier->name + "'", identifier->column);
    }
    return binding->value;
  }
  if (const auto* unary = std::get_if<Unary>(&node.kind)) {
    Result<Value> operand = evaluate(*unary->operand, environment);
    if (!operand) return operand.error();
    return unary->op == '-' ? -operand.value() : operand.value();
  }
  if (const auto* binary = std::get_if<Binary>(&node.kind)) {
    return evaluate_binary(*binary, environment);
  }
  return evaluate_call(std::get<Call>(node.kind), environment);
}

}
