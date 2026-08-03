#include <string>

#include <doctest/doctest.h>

#include "ui/url.hpp"

using namespace calc;

TEST_CASE("a URL is safe only if it is https and one plain word") {
  CHECK(is_safe_url("https://github.com/Thinato/calc"));
  CHECK(is_safe_url("https://example.com"));
  CHECK(is_safe_url("https://sub-domain.example.com/a_b/c.d"));

  SUBCASE("the scheme has to be https") {
    CHECK_FALSE(is_safe_url("http://github.com"));
    CHECK_FALSE(is_safe_url("javascript:alert(1)"));
    CHECK_FALSE(is_safe_url("file:///etc/passwd"));
    CHECK_FALSE(is_safe_url(""));
    CHECK_FALSE(is_safe_url("x https://example.com"));
  }

  SUBCASE("nothing a shell would read as more than a word") {
    CHECK_FALSE(is_safe_url("https://example.com; rm -rf /"));
    CHECK_FALSE(is_safe_url("https://example.com'"));
    CHECK_FALSE(is_safe_url("https://example.com`id`"));
    CHECK_FALSE(is_safe_url("https://example.com$(id)"));
    CHECK_FALSE(is_safe_url("https://example.com&"));
    CHECK_FALSE(is_safe_url("https://example.com two"));
  }

  SUBCASE("and no query or fragment, which are not needed to open a page") {
    CHECK_FALSE(is_safe_url("https://example.com?a=1"));
    CHECK_FALSE(is_safe_url("https://example.com#top"));
  }
}
