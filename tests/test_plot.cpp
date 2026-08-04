#include <cmath>
#include <limits>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "core/plot.hpp"
#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/excmd.hpp"

using namespace calc;

namespace {

class Buffer {
 public:
  explicit Buffer(std::string_view text) : document_(Document::from_text(text)) {
    results_.refresh(document_);
  }

  const Environment& environment() const { return results_.environment(); }

  PlotData plot(const std::string& name, Value from = -10, Value to = 10) const {
    PlotSpec spec;
    spec.name = name;
    spec.x_min = from;
    spec.x_max = to;
    return sample_plot(spec, environment());
  }

 private:
  Document document_;
  ResultCache results_;
};

std::size_t gaps(const PlotData& data) {
  std::size_t count = 0;
  for (const PlotSample& sample : data.samples) {
    if (!sample.ok) ++count;
  }
  return count;
}

}

TEST_CASE("a parabola is sampled across the domain and autoscales") {
  const Buffer buffer("define f(x): x ^ 2\n");
  const PlotData data = buffer.plot("f", -2, 2);

  CHECK(data.samples.size() == kPlotSamples);
  CHECK(data.has_points);
  CHECK(gaps(data) == 0);
  CHECK(data.samples.front().x == doctest::Approx(-2));
  CHECK(data.samples.back().x == doctest::Approx(2));
  CHECK(data.samples.front().y == doctest::Approx(4));
  CHECK(data.samples.back().y == doctest::Approx(4));
  CHECK(data.y_min == doctest::Approx(0).epsilon(0.001));
  CHECK(data.y_max == doctest::Approx(4));
  CHECK_FALSE(data.first_error.has_value());
}

TEST_CASE("a sampled pole leaves a gap rather than a value") {
  const Buffer buffer("define pole(x): 1 / x\n");
  const PlotData data = buffer.plot("pole", 0, 1);

  CHECK(data.has_points);
  CHECK(gaps(data) == 1);
  CHECK_FALSE(data.samples.front().ok);
  CHECK(data.samples.back().y == doctest::Approx(1));
  CHECK_FALSE(data.first_error.has_value());
}

TEST_CASE("a domain that steps over a pole reports the values it did see") {
  const Buffer buffer("define pole(x): 1 / x\n");
  const PlotData data = buffer.plot("pole", -1, 1);

  CHECK(gaps(data) == 0);
  CHECK(data.y_min < -100);
  CHECK(data.y_max > 100);
  CHECK_FALSE(data.first_error.has_value());
}

TEST_CASE("sqrt gaps out below zero") {
  const Buffer buffer("");
  const PlotData data = buffer.plot("sqrt", -4, 4);

  CHECK(data.has_points);
  CHECK(gaps(data) > 0);
  CHECK_FALSE(data.samples.front().ok);
  CHECK(data.samples.back().ok);
  CHECK(data.samples.back().y == doctest::Approx(2));
  REQUIRE(data.first_error.has_value());
  CHECK(data.first_error->code == ErrorCode::DomainError);
}

TEST_CASE("a constant function is padded so it sits mid panel") {
  const Buffer buffer("define flat(x): 3\n");
  const PlotData data = buffer.plot("flat");

  CHECK(data.has_points);
  CHECK(data.y_min == doctest::Approx(2));
  CHECK(data.y_max == doctest::Approx(4));
}

TEST_CASE("a function that fails everywhere keeps its error and no points") {
  const Buffer buffer("define broken(x): x + nope\n");
  const PlotData data = buffer.plot("broken");

  CHECK_FALSE(data.has_points);
  CHECK(gaps(data) == kPlotSamples);
  REQUIRE(data.first_error.has_value());
  CHECK(data.first_error->message == "in broken(): undefined name 'nope'");
  CHECK(data.y_min < data.y_max);
}

TEST_CASE("a body reads the names around it, as it does when called by hand") {
  const Buffer buffer(
      "define with_tax(amount): amount * (1 + RATE)\n"
      "RATE = 0.0825\n");
  const PlotData data = buffer.plot("with_tax", 100, 100.0001);

  CHECK(data.has_points);
  CHECK(data.samples.front().y == doctest::Approx(108.25));
}

TEST_CASE("an explicit y range replaces the autoscaled one") {
  const Buffer buffer("define f(x): x ^ 2\n");

  PlotSpec spec;
  spec.name = "f";
  spec.x_min = -2;
  spec.x_max = 2;
  spec.auto_y = false;
  spec.y_min = -10;
  spec.y_max = 10;

  const PlotData data = sample_plot(spec, buffer.environment());
  CHECK(data.y_min == doctest::Approx(-10));
  CHECK(data.y_max == doctest::Approx(10));
}

TEST_CASE("plot_arity knows builtins, user functions and neither") {
  const Buffer buffer(
      "define f(x): x\n"
      "define hyp(a, b): a + b\n"
      "define answer(): 42\n");

  CHECK(plot_arity("sqrt", buffer.environment()) == 1);
  CHECK(plot_arity("pow", buffer.environment()) == 2);
  CHECK(plot_arity("f", buffer.environment()) == 1);
  CHECK(plot_arity("hyp", buffer.environment()) == 2);
  CHECK(plot_arity("answer", buffer.environment()) == 0);
  CHECK_FALSE(plot_arity("nope", buffer.environment()).has_value());
}

TEST_CASE("plot_dot_row maps top to bottom and stays inside the canvas") {
  PlotData data;
  data.y_min = 0;
  data.y_max = 10;

  CHECK(plot_dot_row(data, 10, 41) == 0);
  CHECK(plot_dot_row(data, 0, 41) == 40);
  CHECK(plot_dot_row(data, 5, 41) == 20);
  CHECK(plot_dot_row(data, 10, 41) < plot_dot_row(data, 5, 41));
  CHECK(plot_dot_row(data, 5, 41) < plot_dot_row(data, 0, 41));
}

TEST_CASE("plot_dot_row clamps a pole instead of running off to infinity") {
  PlotData data;
  data.y_min = -1;
  data.y_max = 1;

  const int rows = 40;
  const Value huge = 1e300;
  const Value nan = std::numeric_limits<Value>::quiet_NaN();
  const Value infinity = std::numeric_limits<Value>::infinity();

  for (const Value y : {huge, -huge, nan, infinity, -infinity, 1e15, -1e15}) {
    const int row = plot_dot_row(data, y, rows);
    CHECK(row >= -1);
    CHECK(row <= rows);
  }

  CHECK(plot_dot_row(data, huge, rows) == -1);
  CHECK(plot_dot_row(data, -huge, rows) == rows);
  CHECK(plot_dot_row(data, nan, rows) == -1);
  CHECK(plot_dot_row(data, 0, 0) == -1);
}

TEST_CASE("a redefined function plots as its last definition") {
  const Buffer buffer(
      "define f(x): x\n"
      "define f(x): x + 100\n");
  const PlotData data = buffer.plot("f", 0, 1);

  CHECK(data.samples.front().y == doctest::Approx(100));
}

namespace {

class Session {
 public:
  explicit Session(std::string_view text) : document_(Document::from_text(text)) {}

  ExOutcome run(const std::string& command, std::size_t row = 0) {
    document_.set_cursor(Cursor{row, 0});
    results_.refresh(document_);
    return execute_ex_command(command, document_, &results_);
  }

  ExOutcome blind(const std::string& command) {
    return execute_ex_command(command, document_);
  }

 private:
  Document document_;
  ResultCache results_;
};

const char* kTargets =
    "define f(x): x ^ 2\n"
    "f(3)\n"
    "\n"
    "define g(x) {\n"
    "  y = x * 2\n"
    "  return y\n"
    "}\n"
    "define hyp(a, b): a + b\n";

}

TEST_CASE("a bare plot finds the definition the cursor is in") {
  Session session(kTargets);

  const ExOutcome from_define = session.run("plot", 0);
  REQUIRE(from_define.plot.has_value());
  CHECK(from_define.plot->name == "f");
  CHECK_FALSE(from_define.is_error);
  CHECK(from_define.plot->x_min == doctest::Approx(-10));
  CHECK(from_define.plot->x_max == doctest::Approx(10));
  CHECK(from_define.plot->auto_y);

  const ExOutcome from_brace = session.run("plot", 3);
  REQUIRE(from_brace.plot.has_value());
  CHECK(from_brace.plot->name == "g");

  const ExOutcome from_body = session.run("plot", 4);
  REQUIRE(from_body.plot.has_value());
  CHECK(from_body.plot->name == "g");
}

TEST_CASE("a bare plot falls back to a function named on the line") {
  Session session(kTargets);

  const ExOutcome from_call = session.run("plot", 1);
  REQUIRE(from_call.plot.has_value());
  CHECK(from_call.plot->name == "f");
}

TEST_CASE("a bare plot on a line with no function says so") {
  Session session(kTargets);

  const ExOutcome outcome = session.run("plot", 2);
  CHECK(outcome.is_error);
  CHECK(outcome.message == "no function on this line: try :plot f");
  CHECK_FALSE(outcome.plot.has_value());
}

TEST_CASE("plot takes a name and one or two ranges") {
  Session session(kTargets);

  const ExOutcome named = session.run("plot g", 2);
  REQUIRE(named.plot.has_value());
  CHECK(named.plot->name == "g");

  const ExOutcome ranged = session.run("plot f -5..5", 2);
  REQUIRE(ranged.plot.has_value());
  CHECK(ranged.plot->x_min == doctest::Approx(-5));
  CHECK(ranged.plot->x_max == doctest::Approx(5));
  CHECK(ranged.plot->auto_y);

  const ExOutcome pinned = session.run("plot f -5..5 0..1", 2);
  REQUIRE(pinned.plot.has_value());
  CHECK_FALSE(pinned.plot->auto_y);
  CHECK(pinned.plot->y_min == doctest::Approx(0));
  CHECK(pinned.plot->y_max == doctest::Approx(1));

  const ExOutcome cursor_ranged = session.run("plot -2..2", 0);
  REQUIRE(cursor_ranged.plot.has_value());
  CHECK(cursor_ranged.plot->name == "f");
  CHECK(cursor_ranged.plot->x_min == doctest::Approx(-2));
}

TEST_CASE("plot rejects what it cannot draw") {
  Session session(kTargets);

  CHECK(session.run("plot nope", 2).message == "unknown function 'nope'");
  CHECK(session.run("plot hyp", 2).message ==
        "plot needs one argument: hyp() takes 2 arguments");
  CHECK(session.run("plot f -5..a", 2).message == "bad range: -5..a");
  CHECK(session.run("plot f 5..5", 2).message == "empty range: 5..5");
  CHECK(session.run("plot f 5..-5", 2).message == "empty range: 5..-5");
  CHECK(session.run("plot f -1..1 0..1 2..3", 2).message == "too many arguments: 2..3");

  for (const char* command : {"plot nope", "plot hyp", "plot f -5..a", "plot f 5..5"}) {
    const ExOutcome outcome = session.run(command, 2);
    CHECK(outcome.is_error);
    CHECK_FALSE(outcome.plot.has_value());
  }
}

TEST_CASE("plot draws a builtin of one argument") {
  Session session(kTargets);

  const ExOutcome outcome = session.run("plot sqrt", 2);
  REQUIRE(outcome.plot.has_value());
  CHECK(outcome.plot->name == "sqrt");
  CHECK(session.run("plot pow", 2).is_error);
}

TEST_CASE("noplot closes the panel and plot does not") {
  Session session(kTargets);

  const ExOutcome closed = session.run("noplot", 0);
  CHECK(closed.close_plot);
  CHECK_FALSE(closed.is_error);
  CHECK_FALSE(closed.plot.has_value());

  CHECK_FALSE(session.run("plot", 0).close_plot);
}

TEST_CASE("without a result cache only the builtins can be plotted") {
  Session session(kTargets);

  const ExOutcome builtin = session.blind("plot sqrt");
  REQUIRE(builtin.plot.has_value());
  CHECK(builtin.plot->name == "sqrt");

  CHECK(session.blind("plot f").message == "unknown function 'f'");
  CHECK(session.blind("plot").is_error);
}
