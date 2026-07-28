#pragma once

#include "doc/document.hpp"

namespace calc {

// Runs the full-screen editor until the user quits. Returns a process exit code.
int run_editor(Document& document);

}  // namespace calc
