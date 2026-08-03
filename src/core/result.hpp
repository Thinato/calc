#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <variant>

namespace calc {

enum class ErrorCode {
  UnexpectedCharacter,
  UnexpectedToken,
  UnexpectedEnd,
  UnbalancedParen,
  ExpectedCallParen,
  UnknownFunction,
  WrongArity,
  DivisionByZero,
  DomainError,
  NotFinite,
  InvalidName,
  UndefinedName,
  ConstantReassigned,
  NameIsFunction,
  AssignmentTarget,
  MultipleAssignment,
  DefineName,
  DuplicateParameter,
  EmptyBody,
  ReturnNotLast,
  ReturnOutsideBody,
  TooMuchRecursion,
  FunctionRedefined,
};

struct Error {
  ErrorCode code{};
  std::string message;
  std::size_t column = 0;

  bool in_body = false;
};

inline Error make_error(ErrorCode code, std::string message, std::size_t column) {
  return Error{code, std::move(message), column};
}

template <typename T>
class Result {
 public:
  Result(T value) : data_(std::move(value)) {}
  Result(Error error) : data_(std::move(error)) {}

  bool ok() const { return std::holds_alternative<T>(data_); }
  explicit operator bool() const { return ok(); }

  T& value() { return std::get<T>(data_); }
  const T& value() const { return std::get<T>(data_); }
  const Error& error() const { return std::get<Error>(data_); }

 private:
  std::variant<T, Error> data_;
};

}
