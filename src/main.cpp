#include <cstdio>
#include <string>
#include <utility>

#include "doc/document.hpp"
#include "doc/file.hpp"
#include "ui/app.hpp"

namespace {

#ifndef CALC_VERSION_STRING
// The build always defines this — from the tag being released, or from the
// version in CMakeLists.txt. Saying "unknown" beats a hardcoded number that
// quietly disagrees with the release it shipped in.
#define CALC_VERSION_STRING "unknown"
#endif

constexpr const char* kVersion = "calc " CALC_VERSION_STRING;

constexpr const char* kUsage =
    "calc — a vim-modal calculator scratchpad\n"
    "\n"
    "usage: calc [file]\n"
    "\n"
    "Every line is an expression; its result is shown after it and cannot be\n"
    "edited. Only the expressions you type are saved.\n"
    "\n"
    "operators   + - * / ^ ( )        functions   pow(a, b)  sqrt(a)\n"
    "comments    # to end of line\n"
    "\n"
    "keys        vim: h j k l w b e 0 ^ $ gg G f t %, d c y with any motion,\n"
    "            i a o O x r ~ J p P, u and Ctrl-R, v and V, / and ? with n N\n"
    "            gy yanks the result of the current line to the clipboard\n"
    "            :w  :wq  :q  :q!  :e file  :set number  :github  :help\n"
    "\n"
    "options     -h, --help       this text\n"
    "            -v, --version    version\n";

}  // namespace

int main(int argc, char** argv) {
  std::string path;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "-h" || argument == "--help") {
      std::fputs(kUsage, stdout);
      return 0;
    }
    if (argument == "-v" || argument == "--version") {
      std::printf("%s\n", kVersion);
      return 0;
    }
    if (!argument.empty() && argument[0] == '-') {
      std::fprintf(stderr, "calc: unknown option: %s\n", argument.c_str());
      return 2;
    }
    if (!path.empty()) {
      std::fputs("calc: only one file can be opened\n", stderr);
      return 2;
    }
    path = argument;
  }

  calc::Document document;
  if (!path.empty()) {
    const calc::ReadOutcome read = calc::read_file(path);
    if (!read.ok) {
      std::fprintf(stderr, "calc: %s\n", read.error.c_str());
      return 1;
    }
    // A file that does not exist yet opens as an empty buffer, as vim does.
    document = calc::Document::from_text(read.contents);
    document.set_path(path);
  }

  return calc::run_editor(std::move(document));
}
