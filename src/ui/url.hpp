#pragma once

#include <algorithm>
#include <string>

namespace calc {

inline bool is_safe_url(const std::string& url) {
  if (url.rfind("https://", 0) != 0) return false;
  return std::all_of(url.begin(), url.end(), [](char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == ':' || byte == '/' || byte == '.' ||
           byte == '-' || byte == '_';
  });
}

}
