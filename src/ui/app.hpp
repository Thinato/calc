#pragma once

#include "doc/document.hpp"

namespace calc {

// Runs the full-screen editor until the user quits. Returns a process exit code.
// Takes the document by value because the editor owns the buffer it edits for as
// long as it runs; the caller has nothing left to read afterwards.
int run_editor(Document document);

}  // namespace calc
