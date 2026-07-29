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
  // Names: variables and constants.
  InvalidName,
  UndefinedName,
  ConstantReassigned,
  NameIsFunction,
  AssignmentTarget,
  MultipleAssignment,
};

// An evaluation failure. `column` is a 0-based byte offset into the source line
// so the status bar can point at the offending character.
struct Error {
  ErrorCode code{};
  std::string message;
  std::size_t column = 0;
};

inline Error make_error(ErrorCode code, std::string message, std::size_t column) {
  return Error{code, std::move(message), column};
}

// A value or an Error. Deliberately not std::expected: that is C++23, and this
// only needs a handful of operations.
template <typename T>
class Result {
 public:
  Result(T value) : data_(std::move(value)) {}      // NOLINT(*-explicit-*)
  Result(Error error) : data_(std::move(error)) {}  // NOLINT(*-explicit-*)

  bool ok() const { return std::holds_alternative<T>(data_); }
  explicit operator bool() const { return ok(); }

  T& value() { return std::get<T>(data_); }
  const T& value() const { return std::get<T>(data_); }
  const Error& error() const { return std::get<Error>(data_); }

 private:
  std::variant<T, Error> data_;
};

}  // namespace calc
