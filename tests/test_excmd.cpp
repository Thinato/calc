#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <vector>

#include "doc/file.hpp"
#include "doc/results.hpp"
#include "helpers/vim_harness.hpp"
#include "vim/engine.hpp"
#include "vim/excmd.hpp"

using namespace calc;

namespace {

// A scratch path that cleans itself up, so the suite leaves no files behind.
class TempFile {
 public:
  explicit TempFile(std::string name)
      : path_((std::filesystem::temp_directory_path() / name).string()) {
    remove();
  }
  ~TempFile() { remove(); }

  TempFile(const TempFile&) = delete;
  TempFile& operator=(const TempFile&) = delete;

  const std::string& path() const { return path_; }

  void write(const std::string& contents) const {
    std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
    stream << contents;
  }

  std::string read() const {
    std::ifstream stream(path_, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
  }

  bool exists() const { return std::filesystem::exists(path_); }

 private:
  void remove() const {
    std::error_code code;
    std::filesystem::remove(path_, code);
  }

  std::string path_;
};

}  // namespace

TEST_CASE(":q refuses to discard unsaved work unless forced") {
  Document document = Document::from_text("1 + 2\n");

  SUBCASE("a clean buffer quits") {
    const ExOutcome outcome = execute_ex_command("q", document);
    CHECK(outcome.quit);
    CHECK_FALSE(outcome.is_error);
  }
  SUBCASE("a modified buffer does not") {
    document.insert_text(Cursor{0, 5}, " + 3");
    const ExOutcome outcome = execute_ex_command("q", document);
    CHECK_FALSE(outcome.quit);
    CHECK(outcome.is_error);
  }
  SUBCASE("but q! does") {
    document.insert_text(Cursor{0, 5}, " + 3");
    const ExOutcome outcome = execute_ex_command("q!", document);
    CHECK(outcome.quit);
  }
}

TEST_CASE(":w writes only the expressions, never the results") {
  // The whole point of keeping results outside the document: a saved file holds
  // exactly what was typed and reopens unchanged.
  const TempFile file("calc_test_write.calc");
  Document document = Document::from_text("1 + 2\nsqrt(16) + pow(2, 10)\n");

  const ExOutcome outcome = execute_ex_command("w " + file.path(), document);
  REQUIRE_FALSE(outcome.is_error);

  const std::string saved = file.read();
  CHECK(saved == "1 + 2\nsqrt(16) + pow(2, 10)\n");
  // No separator, and no trace of either computed value (3 and 1028).
  CHECK(saved.find('=') == std::string::npos);
  CHECK(saved.find("1028") == std::string::npos);
  // The digits the user typed are of course still there.
  CHECK(saved.find("16") != std::string::npos);

  CHECK_FALSE(document.modified());
  CHECK(document.path() == file.path());
}

TEST_CASE(":w with no name reuses the document's path") {
  const TempFile file("calc_test_reuse.calc");
  Document document = Document::from_text("7 * 6\n");
  document.set_path(file.path());

  REQUIRE_FALSE(execute_ex_command("w", document).is_error);
  CHECK(file.read() == "7 * 6\n");
}

TEST_CASE(":w on a nameless buffer is an error") {
  Document document = Document::from_text("1\n");
  const ExOutcome outcome = execute_ex_command("w", document);
  CHECK(outcome.is_error);
  CHECK(outcome.message == "no file name");
}

TEST_CASE(":wq and :x write and quit") {
  const TempFile file("calc_test_wq.calc");
  Document document = Document::from_text("2 ^ 8\n");

  const ExOutcome outcome = execute_ex_command("wq " + file.path(), document);
  CHECK(outcome.quit);
  CHECK_FALSE(outcome.is_error);
  CHECK(file.read() == "2 ^ 8\n");
}

TEST_CASE("a file survives a save and reload unchanged") {
  const TempFile file("calc_test_roundtrip.calc");
  const std::string original = "# shopping\n12.50 * 3\n\n(1 + 2) ^ 2\n";

  Document document = Document::from_text(original);
  REQUIRE_FALSE(execute_ex_command("w " + file.path(), document).is_error);

  Document reloaded;
  REQUIRE_FALSE(execute_ex_command("e " + file.path(), reloaded).is_error);
  CHECK(reloaded.to_text() == original);
}

TEST_CASE(":e opens a file, and a missing one starts empty") {
  SUBCASE("existing") {
    const TempFile file("calc_test_edit.calc");
    file.write("40 + 2\n");

    Document document;
    const ExOutcome outcome = execute_ex_command("e " + file.path(), document);
    CHECK_FALSE(outcome.is_error);
    CHECK(document.line(0) == "40 + 2");
    CHECK(document.path() == file.path());
  }
  SUBCASE("missing files are new buffers, not failures") {
    const TempFile file("calc_test_absent.calc");
    REQUIRE_FALSE(file.exists());

    Document document;
    const ExOutcome outcome = execute_ex_command("e " + file.path(), document);
    CHECK_FALSE(outcome.is_error);
    CHECK(document.empty());
    CHECK(outcome.message.find("new file") != std::string::npos);
  }
  SUBCASE("a modified buffer blocks the reload") {
    Document document = Document::from_text("1\n");
    document.insert_text(Cursor{0, 1}, "2");
    CHECK(execute_ex_command("e somewhere.calc", document).is_error);
  }
}

TEST_CASE(":<number> jumps to a line") {
  Document document = Document::from_text("a\nb\nc\n");
  CHECK(execute_ex_command("2", document).goto_row == 1);
  CHECK(execute_ex_command("1", document).goto_row == 0);
  CHECK(execute_ex_command("0", document).goto_row == 0);
}

TEST_CASE(":set toggles line numbers") {
  Document document;
  CHECK(execute_ex_command("set nonumber", document).line_numbers == false);
  CHECK(execute_ex_command("set number", document).line_numbers == true);
  CHECK(execute_ex_command("set nu", document).line_numbers == true);
  CHECK(execute_ex_command("set nonu", document).line_numbers == false);
  CHECK(execute_ex_command("set wrapscan", document).is_error);
}

TEST_CASE("an unknown command is reported, not silently ignored") {
  Document document;
  const ExOutcome outcome = execute_ex_command("frobnicate", document);
  CHECK(outcome.is_error);
  CHECK(outcome.message.find("frobnicate") != std::string::npos);
}

TEST_CASE("an empty command does nothing") {
  Document document;
  const ExOutcome outcome = execute_ex_command("   ", document);
  CHECK_FALSE(outcome.is_error);
  CHECK_FALSE(outcome.quit);
  CHECK(outcome.message.empty());
}

TEST_CASE(":github asks for the project page to be opened") {
  Document document;
  const ExOutcome outcome = execute_ex_command("github", document);
  CHECK_FALSE(outcome.is_error);
  REQUIRE(outcome.open_url.has_value());
  CHECK(*outcome.open_url == "https://github.com/Thinato/calc");
  // The message names the destination, and nothing else happens to the buffer.
  CHECK(outcome.message == "opening https://github.com/Thinato/calc");
  CHECK_FALSE(outcome.quit);
  CHECK_FALSE(outcome.goto_row.has_value());
  CHECK_FALSE(document.modified());
}

TEST_CASE(":github does not open anything on its own") {
  // The point of returning the URL rather than launching it here: this test runs
  // in CI without a browser appearing.
  Document document;
  CHECK_FALSE(execute_ex_command("w", document).open_url.has_value());
  CHECK_FALSE(execute_ex_command("q", document).open_url.has_value());
}

TEST_CASE("a command is not a prefix of :github") {
  Document document;
  CHECK(execute_ex_command("git", document).is_error);
  CHECK(execute_ex_command("githubs", document).is_error);
  CHECK(execute_ex_command("g", document).is_error);
}

TEST_CASE("the engine hands the URL to the opener it was given") {
  // The wiring is the part that can silently rot: a command that names a URL
  // nobody acts on looks exactly like a working one.
  Document document = Document::from_text("1 + 2");
  ResultCache results;
  VimEngine engine(document, results);

  std::vector<std::string> opened;
  engine.set_url_opener([&](const std::string& url) { opened.push_back(url); });

  for (const Key& key : test::parse_keys(":github<cr>")) engine.feed(key);

  REQUIRE(opened.size() == 1);
  CHECK(opened[0] == "https://github.com/Thinato/calc");
  CHECK(engine.message() == "opening https://github.com/Thinato/calc");
  CHECK_FALSE(engine.message_is_error());
  CHECK(engine.mode() == Mode::Normal);  // and the command line is done with
}

TEST_CASE("an engine with no opener wired up still survives :github") {
  // The default in tests, and on a platform with no opener to offer.
  Document document = Document::from_text("1 + 2");
  ResultCache results;
  VimEngine engine(document, results);

  for (const Key& key : test::parse_keys(":github<cr>")) engine.feed(key);
  CHECK(engine.message() == "opening https://github.com/Thinato/calc");
}

TEST_CASE("reading a directory fails cleanly") {
  const ReadOutcome outcome =
      read_file(std::filesystem::temp_directory_path().string());
  CHECK_FALSE(outcome.ok);
  CHECK_FALSE(outcome.error.empty());
}
