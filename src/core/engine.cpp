#include "core/engine.hpp"

#include <vector>

#include "core/ast.hpp"
#include "core/eval.hpp"
#include "core/format.hpp"
#include "core/lexer.hpp"
#include "core/parser.hpp"

namespace calc {

LineEval evaluate_line(std::string_view line) {
  LineEval outcome;
  if (is_blank_or_comment(line)) return outcome;

  Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) {
    outcome.error = tokens.error();
    return outcome;
  }

  Result<NodePtr> tree = parse(tokens.value());
  if (!tree) {
    outcome.error = tree.error();
    return outcome;
  }

  Result<Value> value = evaluate(*tree.value());
  if (!value) {
    outcome.error = value.error();
    return outcome;
  }

  outcome.value = value.value();
  outcome.text = format_value(value.value());
  return outcome;
}

}  // namespace calc
