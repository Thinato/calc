#include "vim/excmd.hpp"

#include <algorithm>
#include <cctype>

#include "doc/file.hpp"

namespace calc {
namespace {

constexpr std::string_view kUnsavedChanges =
    "no write since last change (add ! to override)";

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
  while (index < text.size() && (std::isalpha(static_cast<unsigned char>(text[index])) != 0)) {
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

  // to_text() serializes the typed lines only; results live outside the
  // document and therefore cannot reach the file.
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

}  // namespace

ExOutcome execute_ex_command(std::string_view command, Document& document) {
  const std::string_view text = trim(command);
  if (text.empty()) return {};

  // `:42` jumps to a line.
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
  if (parsed.name == "h" || parsed.name == "help") {
    ExOutcome outcome;
    outcome.message =
        "calc: type an expression, it evaluates as you go. "
        "vim keys apply. :w writes, :q quits.";
    return outcome;
  }

  return failure("not an editor command: " + std::string(text));
}

}  // namespace calc
