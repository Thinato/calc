#include "core/engine.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "core/ast.hpp"
#include "core/eval.hpp"
#include "core/format.hpp"
#include "core/functions.hpp"
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

  Result<Statement> statement = parse_statement(tokens.value(), &environment);
  if (!statement) {
    outcome.error = statement.error();
    return outcome;
  }

  Statement& parsed = statement.value();

  if (parsed.is_definition()) {
    FunctionDecl& declaration = *parsed.definition;
    auto function = std::make_shared<UserFunction>(
        UserFunction{declaration.name, std::move(declaration.params),
                     std::move(declaration.body), row});

    std::optional<Error> failed =
        environment.define_function(std::move(function), declaration.name_column);
    if (failed) {
      outcome.error = std::move(*failed);
      return outcome;
    }

    outcome.defined_name = declaration.name;
    outcome.defined_column = declaration.name_column;
    return outcome;
  }

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

UnitEval evaluate_unit(const std::vector<std::string>& lines, Unit unit,
                       Environment& environment) {
  if (unit.row_count <= 1) {
    return UnitEval{evaluate_line(lines[unit.first_row], environment, unit.first_row),
                    unit.first_row};
  }

  const std::string joined = join_unit(lines, unit);
  LineEval eval = evaluate_line(joined, environment, unit.first_row);

  std::size_t row = unit.first_row;
  if (eval.error.has_value()) row = row_for_column(lines, unit, eval.error->column);
  return UnitEval{std::move(eval), row};
}

}
