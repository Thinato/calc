#include "core/engine.hpp"

#include <utility>
#include <vector>

#include "core/ast.hpp"
#include "core/eval.hpp"
#include "core/format.hpp"
#include "core/lexer.hpp"
#include "core/parser.hpp"

namespace calc {

LineEval evaluate_line(std::string_view line, Environment& environment, std::size_t row) {
  LineEval outcome;
  if (is_blank_or_comment(line)) return outcome;

  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) {
    outcome.error = tokens.error();
    return outcome;
  }

  Result<Statement> statement = parse_statement(tokens.value());
  if (!statement) {
    outcome.error = statement.error();
    return outcome;
  }

  // Bound once rather than calling statement.value() at every use: it reads
  // better, and it lets a reader — or a static analyser — see that the target
  // checked below is the same optional that is dereferenced.
  const Statement& parsed = statement.value();

  Result<Value> value = evaluate(*parsed.expression, environment);
  if (!value) {
    outcome.error = value.error();
    return outcome;
  }

  if (parsed.target.has_value()) {
    const std::string& name = *parsed.target;
    std::optional<Error> failed =
        environment.define(name, value.value(), row, parsed.target_column);
    if (failed) {
      // The binding keeps whatever it already held, so the lines below still see
      // the value the constant was first given.
      outcome.error = std::move(*failed);
      return outcome;
    }
    outcome.assigned_name = name;
    outcome.assigned_column = parsed.target_column;
    outcome.assigned_constant = Environment::is_constant_name(name);
    outcome.show_result = !is_literal(*parsed.expression);
  }

  outcome.value = value.value();
  outcome.text = format_value(value.value());
  return outcome;
}

LineEval evaluate_line(std::string_view line) {
  Environment environment;
  return evaluate_line(line, environment, 0);
}

}  // namespace calc
