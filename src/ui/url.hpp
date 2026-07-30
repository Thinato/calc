#pragma once

#include <algorithm>
#include <string>

namespace calc {

// A URL safe to hand to something that will act on it: https, and nothing but
// the characters a shell reads as one plain word. Today's only caller passes a
// compile-time constant, so this is not defending against current input — it
// keeps a later caller from turning an opener into command execution with a ';',
// or into script execution with a 'javascript:' URL on the web.
inline bool is_safe_url(const std::string& url) {
  if (url.rfind("https://", 0) != 0) return false;
  return std::all_of(url.begin(), url.end(), [](char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == ':' || byte == '/' || byte == '.' ||
           byte == '-' || byte == '_';
  });
}

}  // namespace calc
