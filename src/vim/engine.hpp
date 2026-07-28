#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "doc/document.hpp"
#include "doc/results.hpp"
#include "vim/keys.hpp"
#include "vim/mode.hpp"
#include "vim/motions.hpp"
#include "vim/operators.hpp"

namespace calc {

// A viewport adjustment the engine wants but cannot make, because only the
// renderer knows how tall the window is.
enum class ScrollRequest {
  None,
  HalfPageDown,
  HalfPageUp,
  LineDown,
  LineUp,
  Center,
  Top,
  Bottom,
};

// The modal editor.
//
// One idea drives the class: operators compose with motions. `dd` is not a
// special case, it is operator `d` over a linewise "current line" motion, and
// `d2w` is the same operator over a counted word motion. Adding a command is
// mostly adding a table entry in motions.cpp rather than a branch here.
class VimEngine {
 public:
  VimEngine(Document& document, const ResultCache& results);

  void feed(const Key& key);

  Mode mode() const { return mode_; }
  bool quit_requested() const { return quit_requested_; }
  bool line_numbers() const { return line_numbers_; }

  // The ':' or '/' line being typed, including its leading character. Empty
  // unless mode() is Mode::CommandLine.
  const std::string& command_line() const { return command_line_; }
  const std::string& message() const { return message_; }
  bool message_is_error() const { return message_is_error_; }
  // A partially typed command such as "2d", shown in the ruler.
  const std::string& pending_keys() const { return pending_keys_; }

  // The visual selection, normalized low-to-high and inclusive of both ends.
  // Empty outside the visual modes.
  std::optional<std::pair<Cursor, Cursor>> selection() const;

  void set_message(std::string text, bool is_error);
  void clear_message();

  // Contents of a register; '"' is the unnamed one. Empty if never written.
  const Register& register_value(char name) const;

  ScrollRequest take_scroll_request();

  // Called with text destined for the system clipboard, for the "+ and "*
  // registers and `gy`. Injected so this layer stays free of platform calls.
  void set_clipboard_writer(std::function<void(const std::string&)> writer);

 private:
  void feed_normal(const Key& key);
  void feed_insert(const Key& key);
  void feed_visual(const Key& key);
  void feed_command_line(const Key& key);

  bool handle_pending_argument(const Key& key);
  bool handle_g_prefix(const Key& key);
  bool handle_z_prefix(const Key& key);
  bool handle_shared_navigation(const Key& key);

  void execute_motion(char key, const std::string& argument);
  void move_cursor(const MotionResult& motion, char key);
  void apply_operator(const Range& range);
  void apply_visual_operator(Operator op);

  void enter_insert();
  void leave_insert();
  void enter_visual(Mode visual_mode);

  void put(bool after);
  void replace_char(const std::string& replacement);
  void toggle_case_range(Cursor from, Cursor to);
  void join_lines_command();
  void yank_result(bool with_expression);
  void repeat_last_change();

  void run_ex(const std::string& command);
  void run_search(const std::string& pattern, bool forward);
  bool search_next(bool forward);

  void store_register(const Register& value);
  const Register& active_register() const;

  void reset_pending();
  bool has_pending() const;
  void refresh_pending_keys();
  int effective_count() const;
  char char_under_cursor() const;
  void clamp_cursor_for_mode();

  Document& document_;
  const ResultCache& results_;

  Mode mode_ = Mode::Normal;
  bool quit_requested_ = false;
  bool line_numbers_ = true;

  // Pending-command state. Together these are the whole parser for normal mode.
  int count_ = 0;              // 0 means the user typed no count
  Operator operator_ = Operator::None;
  char operator_key_ = '\0';   // so a doubled key (dd, yy) is recognizable
  int operator_count_ = 0;
  char register_name_ = '\0';
  char awaiting_argument_ = '\0';  // motion or command waiting for one character
  bool awaiting_register_ = false;
  bool pending_g_ = false;
  bool pending_z_ = false;
  std::string pending_keys_;

  std::size_t desired_column_ = 0;  // remembered column for j and k
  Cursor visual_anchor_;

  std::map<char, Register> registers_;
  std::string command_line_;
  std::string message_;
  bool message_is_error_ = false;
  ScrollRequest scroll_request_ = ScrollRequest::None;

  std::string last_search_;
  bool last_search_forward_ = true;

  // `.` replays the keys of the last command that changed the buffer. Replaying
  // keystrokes rather than reifying commands keeps every command repeatable for
  // free, including a whole insert session.
  std::vector<Key> command_keys_;
  std::vector<Key> last_change_;
  std::size_t command_revision_ = 0;
  bool replaying_ = false;
  bool suppress_record_ = false;

  std::function<void(const std::string&)> clipboard_writer_;
};

}  // namespace calc
