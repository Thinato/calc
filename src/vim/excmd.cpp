#include "vim/excmd.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "core/lexer.hpp"
#include "doc/file.hpp"
#include "doc/results.hpp"

namespace calc {
namespace {

constexpr std::string_view kUnsavedChanges =
    "no write since last change (add ! to override)";

constexpr std::string_view kProjectUrl = "https://github.com/Thinato/calc";

std::string_view trim(std::string_view text) {
  const std::size_t begin = text.find_first_not_of(" \t");
  if (begin == std::string_view::npos) return {};
  const std::size_t end = text.find_last_not_of(" \t");
  return text.substr(begin, end - begin + 1);
}

struct Parsed {
  std::string name;
  bool bang = false;
  std::string argument;
};

Parsed split_command(std::string_view text) {
  Parsed parsed;
  std::size_t index = 0;
  while (index < text.size() &&
         (std::isalpha(static_cast<unsigned char>(text[index])) != 0)) {
    ++index;
  }
  parsed.name = std::string(text.substr(0, index));
  if (index < text.size() && text[index] == '!') {
    parsed.bang = true;
    ++index;
  }
  parsed.argument = std::string(trim(text.substr(index)));
  return parsed;
}

ExOutcome failure(std::string message) {
  ExOutcome outcome;
  outcome.message = std::move(message);
  outcome.is_error = true;
  return outcome;
}

bool all_digits(std::string_view text) {
  return !text.empty() && std::all_of(text.begin(), text.end(), [](char byte) {
    return byte >= '0' && byte <= '9';
  });
}

ExOutcome do_write(Document& document, const Parsed& parsed, bool then_quit) {
  const std::string path = parsed.argument.empty() ? document.path() : parsed.argument;
  if (path.empty()) return failure("no file name");

  const std::string error = write_file(path, document.to_text());
  if (!error.empty()) return failure(error);

  document.set_path(path);
  document.mark_saved();

  ExOutcome outcome;
  outcome.message = "\"" + path + "\" written";
  outcome.quit = then_quit;
  return outcome;
}

ExOutcome do_edit(Document& document, const Parsed& parsed) {
  if (document.modified() && !parsed.bang) return failure(std::string(kUnsavedChanges));

  const std::string path = parsed.argument.empty() ? document.path() : parsed.argument;
  if (path.empty()) return failure("no file name");

  const ReadOutcome read = read_file(path);
  if (!read.ok) return failure(read.error);

  document = Document::from_text(read.contents);
  document.set_path(path);

  ExOutcome outcome;
  outcome.message = read.missing ? "\"" + path + "\" [new file]" : "\"" + path + "\"";
  return outcome;
}

const Environment& plot_environment(const ResultCache* results) {
  static const Environment kEmpty;
  return results != nullptr ? results->environment() : kEmpty;
}

std::vector<std::string_view> words_of(std::string_view text) {
  std::vector<std::string_view> words;
  std::size_t index = 0;
  while (index < text.size()) {
    while (index < text.size() && (text[index] == ' ' || text[index] == '\t')) ++index;
    const std::size_t start = index;
    while (index < text.size() && text[index] != ' ' && text[index] != '\t') ++index;
    if (index > start) words.push_back(text.substr(start, index - start));
  }
  return words;
}

std::optional<Value> to_number(std::string_view text) {
  if (text.empty()) return std::nullopt;
  const std::string owned(text);
  char* end = nullptr;
  const Value value = std::strtod(owned.c_str(), &end);
  if (end != owned.c_str() + owned.size()) return std::nullopt;
  if (!std::isfinite(value)) return std::nullopt;
  return value;
}

struct Bounds {
  Value min = 0;
  Value max = 0;
};

std::optional<Bounds> to_bounds(std::string_view text) {
  const std::size_t dots = text.find("..");
  if (dots == std::string_view::npos) return std::nullopt;
  const std::optional<Value> min = to_number(text.substr(0, dots));
  const std::optional<Value> max = to_number(text.substr(dots + 2));
  if (!min.has_value() || !max.has_value()) return std::nullopt;
  return Bounds{*min, *max};
}

std::optional<std::string> function_named_on(const std::string& line,
                                             const Environment& environment) {
  const Result<std::vector<Token>> tokens = tokenize(line);
  if (!tokens) return std::nullopt;
  for (const Token& token : tokens.value()) {
    if (token.kind != TokenKind::Identifier) continue;
    if (plot_arity(token.text, environment) == 1) return token.text;
  }
  return std::nullopt;
}

ExOutcome do_plot(const Document& document, const Parsed& parsed,
                  const ResultCache* results) {
  const Environment& environment = plot_environment(results);
  const std::vector<std::string_view> words = words_of(parsed.argument);

  PlotSpec spec;
  std::size_t index = 0;

  if (index < words.size() && !to_bounds(words[index]).has_value()) {
    spec.name = std::string(words[index]);
    ++index;
  } else {
    const std::size_t row = document.cursor().row;
    std::optional<std::string> found =
        results != nullptr ? results->definition_at(row) : std::nullopt;
    if (!found.has_value()) found = function_named_on(document.line(row), environment);
    if (!found.has_value()) return failure("no function on this line: try :plot f");
    spec.name = *found;
  }

  for (int axis = 0; axis < 2 && index < words.size(); ++axis) {
    const std::optional<Bounds> bounds = to_bounds(words[index]);
    if (!bounds.has_value()) {
      return failure("bad range: " + std::string(words[index]));
    }
    if (!(bounds->min < bounds->max)) {
      return failure("empty range: " + std::string(words[index]));
    }
    if (axis == 0) {
      spec.x_min = bounds->min;
      spec.x_max = bounds->max;
    } else {
      spec.auto_y = false;
      spec.y_min = bounds->min;
      spec.y_max = bounds->max;
    }
    ++index;
  }

  if (index < words.size()) {
    return failure("too many arguments: " + std::string(words[index]));
  }

  const std::optional<std::size_t> arity = plot_arity(spec.name, environment);
  if (!arity.has_value()) return failure("unknown function '" + spec.name + "'");
  if (*arity != 1) {
    return failure("plot needs one argument: " + spec.name + "() takes " +
                   std::to_string(*arity) + (*arity == 1 ? " argument" : " arguments"));
  }

  ExOutcome outcome;
  outcome.plot = std::move(spec);
  return outcome;
}

ExOutcome do_set(const Parsed& parsed) {
  ExOutcome outcome;
  const std::string& option = parsed.argument;
  if (option == "number" || option == "nu") {
    outcome.line_numbers = true;
    return outcome;
  }
  if (option == "nonumber" || option == "nonu") {
    outcome.line_numbers = false;
    return outcome;
  }
  return failure("unknown option: " + option);
}

}

ExOutcome execute_ex_command(std::string_view command, Document& document,
                             const ResultCache* results) {
  const std::string_view text = trim(command);
  if (text.empty()) return {};

  if (all_digits(text)) {
    ExOutcome outcome;
    const std::size_t line = static_cast<std::size_t>(std::stoul(std::string(text)));
    outcome.goto_row = line == 0 ? 0 : line - 1;
    return outcome;
  }

  const Parsed parsed = split_command(text);

  if (parsed.name == "w" || parsed.name == "write") {
    return do_write(document, parsed, false);
  }
  if (parsed.name == "wq" || parsed.name == "x" || parsed.name == "xit") {
    return do_write(document, parsed, true);
  }
  if (parsed.name == "q" || parsed.name == "quit" || parsed.name == "qa" ||
      parsed.name == "qall") {
    if (document.modified() && !parsed.bang) return failure(std::string(kUnsavedChanges));
    ExOutcome outcome;
    outcome.quit = true;
    return outcome;
  }
  if (parsed.name == "e" || parsed.name == "edit") {
    return do_edit(document, parsed);
  }
  if (parsed.name == "set" || parsed.name == "se") {
    return do_set(parsed);
  }
  if (parsed.name == "plot") {
    return do_plot(document, parsed, results);
  }
  if (parsed.name == "noplot") {
    ExOutcome outcome;
    outcome.close_plot = true;
    return outcome;
  }
  if (parsed.name == "github") {
    ExOutcome outcome;
    outcome.open_url = std::string(kProjectUrl);
    outcome.message = "opening " + std::string(kProjectUrl);
    return outcome;
  }
  if (parsed.name == "h" || parsed.name == "help") {
    ExOutcome outcome;
    outcome.message =
        "calc: type an expression, it evaluates as you go. "
        "vim keys apply. :w writes, :q quits, :plot charts a function.";
    return outcome;
  }

  return failure("not an editor command: " + std::string(text));
}

}
