#pragma once

#include <string_view>
#include <vector>

#include "core/ast.hpp"
#include "core/lexer.hpp"
#include "core/result.hpp"

namespace calc {

Result<NodePtr> parse(const std::vector<Token>& tokens);

Result<NodePtr> parse(std::string_view line);

Result<Statement> parse_statement(const std::vector<Token>& tokens);
Result<Statement> parse_statement(std::string_view line);

}
