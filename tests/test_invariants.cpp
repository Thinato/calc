#include <cstdint>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "doc/utf8.hpp"
#include "helpers/vim_harness.hpp"
#include "vim/engine.hpp"

using namespace calc;
using calc::test::apply;

namespace {

class Rng {
 public:
  explicit Rng(std::uint64_t seed) : state_(seed) {}
  std::uint64_t next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 7;
    state_ ^= state_ << 17;
    return state_;
  }
  std::size_t below(std::size_t limit) {
    return static_cast<std::size_t>(next() % limit);
  }

 private:
  std::uint64_t state_;
};

const std::vector<Key>& key_alphabet() {
  static const std::vector<Key> kKeys = [] {
    std::vector<Key> keys;
    for (char byte : std::string("hjkl0^$wbeWBEGgfFtT%{}|dcyxXsDCSYpPurR~Jv"
                                 "V.nN123456789\"aiIoOAelq:/?+-*^()= 	")) {
      keys.push_back(Key::character(byte));
    }
    keys.push_back(Key::special(Key::Type::Escape));
    keys.push_back(Key::special(Key::Type::Enter));
    keys.push_back(Key::special(Key::Type::Backspace));
    keys.push_back(Key::special(Key::Type::Delete));
    keys.push_back(Key::special(Key::Type::Tab));
    keys.push_back(Key::special(Key::Type::Left));
    keys.push_back(Key::special(Key::Type::Right));
    keys.push_back(Key::special(Key::Type::Up));
    keys.push_back(Key::special(Key::Type::Down));
    keys.push_back(Key::special(Key::Type::Home));
    keys.push_back(Key::special(Key::Type::End));
    keys.push_back(Key::control('r'));
    keys.push_back(Key::control('d'));
    keys.push_back(Key::control('u'));
    return keys;
  }();
  return kKeys;
}

}

TEST_CASE("no key sequence can reach the result column") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    Rng rng(seed * 0x9E3779B97F4A7C15ULL);

    Document document = Document::from_text("1 + 2\nsqrt(16)\n\n# note\n7 / 2");
    ResultCache results;
    results.refresh(document);
    VimEngine engine(document, results);

    for (int step = 0; step < 120; ++step) {
      engine.feed(key_alphabet()[rng.below(key_alphabet().size())]);
      results.refresh(document);

      const Cursor cursor = document.cursor();

      REQUIRE(cursor.row < document.line_count());

      REQUIRE(cursor.column <= document.line_length(cursor.row));

      if (engine.mode() != Mode::Insert && document.line_length(cursor.row) > 0) {
        REQUIRE(cursor.column < document.line_length(cursor.row));
      }

      const std::string& line = document.line(cursor.row);
      if (cursor.column < line.size()) {
        REQUIRE_FALSE(utf8::is_continuation(line[cursor.column]));
      }

      REQUIRE(document.line_count() >= 1);
    }
  }
}

TEST_CASE("neither a result nor an error leaks into the buffer text") {
  for (std::uint64_t seed = 1; seed <= 200; ++seed) {
    Rng rng(seed * 0xD1B54A32D192ED03ULL);

    Document document = Document::from_text("1 + 2\n2 * 3\n10 / 4");
    ResultCache results;
    results.refresh(document);
    VimEngine engine(document, results);

    for (int step = 0; step < 120; ++step) {
      engine.feed(key_alphabet()[rng.below(key_alphabet().size())]);
      results.refresh(document);
    }

    const auto ends_with = [](const std::string& line, const std::string& tail) {
      return line.size() >= tail.size() &&
             line.compare(line.size() - tail.size(), tail.size(), tail) == 0;
    };

    for (std::size_t row = 0; row < document.line_count(); ++row) {
      const LineEval& eval = results.at(row);
      const std::string& line = document.line(row);
      CAPTURE(line);

      if (eval.has_result()) {
        const std::string rendered = " = " + eval.text;
        CAPTURE(rendered);
        REQUIRE_FALSE(ends_with(line, rendered));
      }
      if (eval.error.has_value()) {
        const std::string rendered = "  Error: " + eval.error->message;
        CAPTURE(rendered);
        REQUIRE_FALSE(ends_with(line, rendered));
      }
    }
  }
}

TEST_CASE("$ and A land at the end of the expression, not the result") {
  CHECK(apply("1 + 2", "$").cursor.column == 4);
  CHECK(apply("1 + 2", "A").cursor.column == 5);
  CHECK(apply("1 + 2", "Ax<esc>").buffer == "1 + 2x");
  CHECK(apply("1 + 2", "$x").buffer == "1 + ");
}

TEST_CASE("deleting to the end of a line stops at the typed text") {
  CHECK(apply("1 + 2", "D").buffer == "");
  CHECK(apply("1 + 2", "d$").buffer == "");
  CHECK(apply("1 + 2", "v$d").buffer == "");
  CHECK(apply("1 + 2", "D").cursor.column == 0);
}

TEST_CASE("saving a buffer full of results writes only the expressions") {
  Document document = Document::from_text("1 + 2\nsqrt(16)\npow(2, 10)\n");
  ResultCache results;
  results.refresh(document);

  REQUIRE(results.at(0).text == "3");
  REQUIRE(results.at(1).text == "4");
  REQUIRE(results.at(2).text == "1024");

  CHECK(document.to_text() == "1 + 2\nsqrt(16)\npow(2, 10)\n");
}
