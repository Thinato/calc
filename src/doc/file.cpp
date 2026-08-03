#include "doc/file.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace calc {

ReadOutcome read_file(const std::string& path) {
  ReadOutcome outcome;

  std::error_code code;
  if (!std::filesystem::exists(path, code)) {
    outcome.ok = true;
    outcome.missing = true;
    return outcome;
  }
  if (std::filesystem::is_directory(path, code)) {
    outcome.error = path + " is a directory";
    return outcome;
  }

  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    outcome.error = "cannot read " + path + ": " + std::strerror(errno);
    return outcome;
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  outcome.ok = true;
  outcome.contents = buffer.str();
  return outcome;
}

std::string write_file(const std::string& path, const std::string& contents) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return "cannot write " + path + ": " + std::strerror(errno);
  }
  stream << contents;
  stream.flush();
  if (!stream) {
    return "cannot write " + path + ": " + std::strerror(errno);
  }
  return {};
}

}
