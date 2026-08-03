#pragma once

#include <string>

namespace calc {

struct ReadOutcome {
  bool ok = false;
  bool missing = false;
  std::string contents;
  std::string error;
};

ReadOutcome read_file(const std::string& path);

std::string write_file(const std::string& path, const std::string& contents);

}
