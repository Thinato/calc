#pragma once

#include <string>

namespace calc {

struct ReadOutcome {
  bool ok = false;
  bool missing = false;  // no such file: opening a new buffer, not a failure
  std::string contents;
  std::string error;
};

ReadOutcome read_file(const std::string& path);

// Returns an empty string on success, or a message describing the failure.
std::string write_file(const std::string& path, const std::string& contents);

}  // namespace calc
