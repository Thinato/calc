#include <doctest/doctest.h>

#include "doc/document.hpp"
#include "doc/results.hpp"

using namespace calc;

TEST_CASE("a fresh document holds one empty line") {
  const Document document;
  CHECK(document.line_count() == 1);
  CHECK(document.line(0).empty());
  CHECK(document.empty());
}

TEST_CASE("text round-trips through the document") {
  SUBCASE("with a trailing newline") {
    const Document document = Document::from_text("1 + 2\n3 * 4\n");
    REQUIRE(document.line_count() == 2);
    CHECK(document.line(0) == "1 + 2");
    CHECK(document.line(1) == "3 * 4");
    CHECK(document.to_text() == "1 + 2\n3 * 4\n");
  }
  SUBCASE("without a trailing newline") {
    const Document document = Document::from_text("1 + 2\n3 * 4");
    REQUIRE(document.line_count() == 2);
    CHECK(document.to_text() == "1 + 2\n3 * 4\n");
  }
  SUBCASE("blank lines in the middle survive") {
    const Document document = Document::from_text("1\n\n2\n");
    REQUIRE(document.line_count() == 3);
    CHECK(document.line(1).empty());
  }
  SUBCASE("an empty file is one empty line") {
    const Document document = Document::from_text("");
    CHECK(document.line_count() == 1);
  }
}

TEST_CASE("the cursor can never leave the typed text") {
  Document document = Document::from_text("1 + 2\nab\n");

  SUBCASE("a column past the end of a line is pulled back") {
    document.set_cursor(Cursor{1, 99});
    CHECK(document.cursor().row == 1);
    CHECK(document.cursor().column == 2);  // one past 'b', the insert limit
  }
  SUBCASE("a row past the end of the buffer is pulled back") {
    document.set_cursor(Cursor{99, 0});
    CHECK(document.cursor().row == 1);
  }
  SUBCASE("normal mode stops on the last character, not past it") {
    CHECK(document.clamped(Cursor{1, 99}, false).column == 1);  // on 'b'
    CHECK(document.clamped(Cursor{1, 99}, true).column == 2);   // after 'b'
  }
  SUBCASE("an empty line clamps to column zero either way") {
    Document blank = Document::from_text("\n");
    CHECK(blank.clamped(Cursor{0, 5}, false).column == 0);
    CHECK(blank.clamped(Cursor{0, 5}, true).column == 0);
  }
}

TEST_CASE("the cursor never lands inside a multi-byte character") {
  Document document = Document::from_text("# café\n");
  // 'é' occupies two bytes, so column 6 is its continuation byte.
  const Cursor clamped = document.clamped(Cursor{0, 6}, true);
  CHECK(clamped.column == 5);
}

TEST_CASE("charwise erase across lines joins them") {
  Document document = Document::from_text("abc\ndef\n");
  const std::string removed = document.erase_range(Cursor{0, 1}, Cursor{1, 2});
  CHECK(removed == "bc\nde");
  CHECK(document.to_text() == "af\n");
}

TEST_CASE("erasing every line leaves one empty line behind") {
  Document document = Document::from_text("a\nb\n");
  document.erase_lines(0, 2);
  CHECK(document.line_count() == 1);
  CHECK(document.line(0).empty());
}

TEST_CASE("multiline insertion splits lines and reports its end") {
  Document document = Document::from_text("abcd\n");
  const Cursor end = document.insert_text_multiline(Cursor{0, 2}, "X\nY");
  CHECK(document.to_text() == "abX\nYcd\n");
  CHECK(end.row == 1);
  CHECK(end.column == 1);
}

TEST_CASE("undo collapses everything between begin and commit") {
  Document document = Document::from_text("1 + 2\n");

  document.begin_change();
  document.insert_text(Cursor{0, 5}, " + 3");
  document.insert_text(Cursor{0, 9}, " + 4");
  document.commit_change();
  CHECK(document.line(0) == "1 + 2 + 3 + 4");

  REQUIRE(document.undo());
  CHECK(document.line(0) == "1 + 2");

  REQUIRE(document.redo());
  CHECK(document.line(0) == "1 + 2 + 3 + 4");
}

TEST_CASE("undo returns false when there is nothing left to undo") {
  Document document = Document::from_text("x\n");
  CHECK_FALSE(document.undo());
}

TEST_CASE("a new edit discards the redo history") {
  Document document = Document::from_text("a\n");
  document.begin_change();
  document.insert_text(Cursor{0, 1}, "b");
  document.commit_change();
  REQUIRE(document.undo());

  document.begin_change();
  document.insert_text(Cursor{0, 1}, "c");
  document.commit_change();
  CHECK_FALSE(document.redo());
  CHECK(document.line(0) == "ac");
}

TEST_CASE("the modified flag tracks unsaved work") {
  Document document = Document::from_text("1\n");
  CHECK_FALSE(document.modified());
  document.insert_text(Cursor{0, 1}, "2");
  CHECK(document.modified());
  document.mark_saved();
  CHECK_FALSE(document.modified());
}

TEST_CASE("the revision counter moves on every mutation") {
  Document document = Document::from_text("1\n");
  const std::size_t before = document.revision();
  document.set_cursor(Cursor{0, 0});
  CHECK(document.revision() == before);  // moving the cursor is not a change
  document.insert_text(Cursor{0, 1}, "2");
  CHECK(document.revision() != before);
}

TEST_CASE("the result cache follows the text it was built from") {
  Document document = Document::from_text("1 + 2\nhello\n\n");
  ResultCache results;
  results.refresh(document);

  CHECK(results.at(0).text == "3");
  CHECK_FALSE(results.at(1).has_result());   // not an expression
  CHECK(results.at(1).error.has_value());
  CHECK_FALSE(results.at(2).has_result());   // blank line
  CHECK_FALSE(results.at(2).error.has_value());

  document.replace_line(0, "2 * 21");
  results.refresh(document);
  CHECK(results.at(0).text == "42");
}

TEST_CASE("the result cache re-evaluates when lines shift") {
  Document document = Document::from_text("1 + 1\n2 + 2\n");
  ResultCache results;
  results.refresh(document);
  CHECK(results.at(0).text == "2");

  document.erase_lines(0, 1);
  results.refresh(document);
  CHECK(results.at(0).text == "4");  // row 0 is now the old row 1
}
