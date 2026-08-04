#include "core/eval.hpp"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "core/format.hpp"
#include "core/functions.hpp"

namespace calc {
namespace {

constexpr std::size_t kMaxCallDepth = 64;

Error undefined(std::size_t column) {
  return make_error(ErrorCode::DomainError, "result is undefined", column);
}

Result<Value> checked(Value value, std::size_t column, InfinityMode mode,
                      bool explained) {
  if (std::isnan(value)) return undefined(column);
  if (std::isinf(value)) {
    if (!explained) {
      return make_error(ErrorCode::NotFinite, "result is too large", column);
    }
    return infinity_for(mode, value);
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
  const InfinityMode mode = environment.infinity_mode();
  const bool explained = std::isinf(a) || std::isinf(b);
  const bool unbounded_sum =
      mode == InfinityMode::Projective && std::isinf(a) && std::isinf(b);

  switch (node.op) {
    case '+':
      if (unbounded_sum) return undefined(node.column);
      return checked(a + b, node.column, mode, explained);
    case '-':
      if (unbounded_sum) return undefined(node.column);
      return checked(a - b, node.column, mode, explained);
    case '*': return checked(a * b, node.column, mode, explained);
    case '/':
      if (b == 0) {
        if (a == 0) {
          return make_error(ErrorCode::DivisionByZero, "division by zero", node.column);
        }
        return infinity_for(mode, a);
      }
      return checked(a / b, node.column, mode, explained);
    case '^': {
      if (a == 0 && b < 0) return infinity_for(mode, a);
      const Value result = std::pow(a, b);
      if (std::isnan(result) && !std::isnan(a) && !std::isnan(b)) {
        return make_error(ErrorCode::DomainError, "'^' is undefined here", node.column);
      }
      return checked(result, node.column, mode, explained);
    }
    default: break;
  }
  return make_error(ErrorCode::UnexpectedToken, "unsupported operator", node.column);
}

Error from_body(const Error& error, const std::string& name, std::size_t column) {
  Error wrapped = error;
  if (!wrapped.in_body) {
    wrapped.message = "in " + name + "(): " + wrapped.message;
    wrapped.in_body = true;
  }
  wrapped.column = column;
  return wrapped;
}

Result<Value> evaluate_user_call(const UserFunction& function, const std::string& name,
                                 std::size_t column, const std::vector<Value>& args,
                                 const Environment& environment) {
  if (environment.depth() >= kMaxCallDepth) {
    Error error = make_error(ErrorCode::TooMuchRecursion,
                             "too much recursion in '" + function.name + "'", column);
    error.in_body = true;
    return error;
  }

  Environment scope = environment.child_for_call();
  for (std::size_t index = 0; index < function.params.size(); ++index) {
    const Param& param = function.params[index];
    if (std::optional<Error> failed =
            scope.define(param.name, args[index], function.defined_row, param.column)) {
      return from_body(*failed, name, column);
    }
  }

  Value last = 0;
  for (const Statement& statement : function.body) {
    Result<Value> value = evaluate(*statement.expression, scope);
    if (!value) return from_body(value.error(), name, column);
    if (statement.target.has_value()) {
      if (std::optional<Error> failed =
              scope.define(*statement.target, value.value(), function.defined_row,
                           statement.target_column)) {
        return from_body(*failed, name, column);
      }
    }
    last = value.value();
  }
  return last;
}

struct Callee {
  const FunctionDef* builtin = nullptr;
  const UserFunction* user = nullptr;
};

Result<Callee> resolve_function(const std::string& name, std::size_t args_size,
                                std::size_t column, const Environment& environment) {
  Callee callee;
  callee.builtin = find_function(name);
  if (callee.builtin == nullptr) callee.user = environment.find_user_function(name);
  if (callee.builtin == nullptr && callee.user == nullptr) {
    return make_error(ErrorCode::UnknownFunction, "unknown function '" + name + "'",
                      column);
  }

  const std::size_t arity =
      callee.builtin != nullptr ? callee.builtin->arity : callee.user->params.size();
  if (args_size != arity) {
    return make_error(ErrorCode::WrongArity,
                      name + "() takes " + std::to_string(arity) +
                          (arity == 1 ? " argument, got " : " arguments, got ") +
                          std::to_string(args_size),
                      column);
  }
  return callee;
}

Result<Value> invoke(const Callee& callee, const std::string& name,
                     const std::vector<Value>& args, std::size_t column,
                     const Environment& environment) {
  Result<Value> result =
      callee.user != nullptr
          ? evaluate_user_call(*callee.user, name, column, args, environment)
          : callee.builtin->apply(args, column, environment.infinity_mode());
  if (!result) return result.error();
  if (std::isnan(result.value())) return undefined(column);
  return normalized(result.value(), environment.infinity_mode());
}

Result<Value> apply_function(const std::string& name, const std::vector<Value>& args,
                             std::size_t column, const Environment& environment) {
  Result<Callee> callee = resolve_function(name, args.size(), column, environment);
  if (!callee) return callee.error();
  return invoke(callee.value(), name, args, column, environment);
}

Result<Value> evaluate_call(const Call& node, const Environment& environment) {
  Result<Callee> callee =
      resolve_function(node.name, node.args.size(), node.column, environment);
  if (!callee) return callee.error();

  std::vector<Value> args;
  args.reserve(node.args.size());
  for (const NodePtr& argument : node.args) {
    Result<Value> value = evaluate(*argument, environment);
    if (!value) return value.error();
    args.push_back(value.value());
  }
  return invoke(callee.value(), node.name, args, node.column, environment);
}

Result<Value> too_many_terms(std::size_t column) {
  return make_error(ErrorCode::SumRange,
                    "sum has too many terms (limit " + std::to_string(kMaxSumTerms) + ")",
                    column);
}

Result<Value> sum_over(const Sum& node, const Environment& environment,
                       std::vector<Value>& args) {
  Result<Value> first = evaluate(*node.first, environment);
  if (!first) return first.error();
  Result<Value> last = evaluate(*node.last, environment);
  if (!last) return last.error();

  const Value from = first.value();
  const Value to = last.value();

  if (!std::isfinite(from) || !std::isfinite(to)) {
    return make_error(ErrorCode::SumRange, "sum needs a finite range", node.column);
  }
  for (const Value bound : {from, to}) {
    if (bound != std::floor(bound)) {
      return make_error(ErrorCode::SumRange,
                        "sum needs whole numbers, got " + format_value(bound),
                        node.column);
    }
  }
  if (to < from) return Value{0};

  const Value terms = to - from + 1;
  if (terms > static_cast<Value>(kMaxSumTerms)) return too_many_terms(node.column);

  const std::size_t count = static_cast<std::size_t>(terms);
  if (!environment.spend_sum_terms(count)) return too_many_terms(node.column);

  const auto* nested = std::get_if<Sum>(&node.closure->kind);
  Value total = 0;
  bool explained = false;

  for (std::size_t step = 0; step < count; ++step) {
    args.push_back(from + static_cast<Value>(step));
    Result<Value> term = nested != nullptr
                             ? sum_over(*nested, environment, args)
                             : apply_function(std::get<FuncRef>(node.closure->kind).name,
                                              args, node.column, environment);
    args.pop_back();
    if (!term) return term.error();

    explained = explained || std::isinf(term.value());
    total += term.value();
  }
  return checked(total, node.column, environment.infinity_mode(), explained);
}

Result<Value> evaluate_sum(const Sum& node, const Environment& environment) {
  std::vector<Value> args;
  if (environment.has_sum_budget()) return sum_over(node, environment, args);

  const Environment budgeted = environment.with_sum_budget();
  return sum_over(node, budgeted, args);
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
    const Value value = unary->op == '-' ? -operand.value() : operand.value();
    return normalized(value, environment.infinity_mode());
  }
  if (const auto* binary = std::get_if<Binary>(&node.kind)) {
    return evaluate_binary(*binary, environment);
  }
  if (const auto* call = std::get_if<Call>(&node.kind)) {
    return evaluate_call(*call, environment);
  }
  if (const auto* sum = std::get_if<Sum>(&node.kind)) {
    return evaluate_sum(*sum, environment);
  }
  const FuncRef& reference = std::get<FuncRef>(node.kind);
  return make_error(ErrorCode::NameIsFunction,
                    "'" + reference.name + "' is a function, not a value",
                    reference.column);
}

}
