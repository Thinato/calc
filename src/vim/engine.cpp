#include "vim/engine.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "doc/utf8.hpp"
#include "vim/excmd.hpp"

namespace calc {
namespace {

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t newline = text.find('\n', start);
    if (newline == std::string::npos) {
      if (start < text.size()) lines.emplace_back(text.substr(start));
      break;
    }
    lines.emplace_back(text.substr(start, newline - start));
    start = newline + 1;
  }
  return lines;
}

std::string repeat(const std::string& text, int times) {
  std::string result;
  result.reserve(text.size() * static_cast<std::size_t>(std::max(times, 1)));
  for (int step = 0; step < std::max(times, 1); ++step) result += text;
  return result;
}

std::string toggle_case(std::string text) {
  for (char& byte : text) {
    const auto value = static_cast<unsigned char>(byte);
    if (std::islower(value) != 0) {
      byte = static_cast<char>(std::toupper(value));
    } else if (std::isupper(value) != 0) {
      byte = static_cast<char>(std::tolower(value));
    }
  }
  return text;
}

Operator operator_for(char key) {
  switch (key) {
    case 'd': return Operator::Delete;
    case 'c': return Operator::Change;
    case 'y': return Operator::Yank;
    default: return Operator::None;
  }
}

}  // namespace

VimEngine::VimEngine(Document& document, const ResultCache& results)
    : document_(document), results_(results) {}

void VimEngine::set_clipboard_writer(std::function<void(const std::string&)> writer) {
  clipboard_writer_ = std::move(writer);
}

void VimEngine::set_url_opener(std::function<void(const std::string&)> opener) {
  url_opener_ = std::move(opener);
}

void VimEngine::set_message(std::string text, bool is_error) {
  message_ = std::move(text);
  message_is_error_ = is_error;
}

void VimEngine::clear_message() {
  message_.clear();
  message_is_error_ = false;
}

ScrollRequest VimEngine::take_scroll_request() {
  return std::exchange(scroll_request_, ScrollRequest::None);
}

std::optional<std::pair<Cursor, Cursor>> VimEngine::selection() const {
  if (mode_ != Mode::Visual && mode_ != Mode::VisualLine) return std::nullopt;
  Cursor low = visual_anchor_;
  Cursor high = document_.cursor();
  if (high < low) std::swap(low, high);
  return std::make_pair(low, high);
}

// ---------------------------------------------------------------- entry point

void VimEngine::feed(const Key& key) {
  // Accumulate the keys of the current command so `.` can replay it. A command
  // boundary is "back in normal mode with nothing pending".
  if (!replaying_) {
    if (mode_ == Mode::Normal && !has_pending()) {
      command_keys_.clear();
      // The revision is sampled at the start of the whole command, not of this
      // keystroke: the Esc that ends an insert session changes nothing by
      // itself, so a per-keystroke comparison would never record the insert.
      command_revision_ = document_.revision();
    }
    command_keys_.push_back(key);
  }

  suppress_record_ = false;

  switch (mode_) {
    case Mode::Normal: feed_normal(key); break;
    case Mode::Insert: feed_insert(key); break;
    case Mode::Visual:
    case Mode::VisualLine: feed_visual(key); break;
    case Mode::CommandLine: feed_command_line(key); break;
  }

  const bool command_complete = mode_ == Mode::Normal && !has_pending();
  if (!replaying_ && !suppress_record_ && command_complete &&
      document_.revision() != command_revision_) {
    last_change_ = command_keys_;
  }
  refresh_pending_keys();
}

// ------------------------------------------------------------- pending state

bool VimEngine::has_pending() const {
  return count_ != 0 || operator_ != Operator::None || awaiting_argument_ != '\0' ||
         awaiting_register_ || pending_g_ || pending_z_;
}

void VimEngine::reset_pending() {
  count_ = 0;
  operator_ = Operator::None;
  operator_key_ = '\0';
  operator_count_ = 0;
  register_name_ = '\0';
  awaiting_argument_ = '\0';
  awaiting_register_ = false;
  pending_g_ = false;
  pending_z_ = false;
}

void VimEngine::refresh_pending_keys() {
  pending_keys_.clear();
  if (register_name_ != '\0') {
    pending_keys_ += '"';
    pending_keys_ += register_name_;
  }
  if (operator_count_ > 0) pending_keys_ += std::to_string(operator_count_);
  if (operator_key_ != '\0') pending_keys_ += operator_key_;
  if (count_ > 0) pending_keys_ += std::to_string(count_);
  if (pending_g_) pending_keys_ += 'g';
  if (pending_z_) pending_keys_ += 'z';
  if (awaiting_argument_ != '\0') pending_keys_ += awaiting_argument_;
}

int VimEngine::effective_count() const { return count_ > 0 ? count_ : 1; }

char VimEngine::char_under_cursor() const {
  const Cursor cursor = document_.cursor();
  if (cursor.column >= document_.line_length(cursor.row)) return '\0';
  return document_.line(cursor.row)[cursor.column];
}

void VimEngine::clamp_cursor_for_mode() {
  document_.set_cursor(document_.clamped(document_.cursor(), allows_cursor_past_end(mode_)));
}

// ------------------------------------------------------------------ registers

void VimEngine::store_register(const Register& value) {
  const char name = register_name_;

  if (name == '+' || name == '*') {
    if (clipboard_writer_) clipboard_writer_(value.text);
  } else if (name >= 'A' && name <= 'Z') {
    // An uppercase register name appends, as in vim.
    Register& target = registers_[static_cast<char>(name - 'A' + 'a')];
    target.text += value.text;
    target.linewise = value.linewise;
  } else if (name != '\0' && name != '"') {
    registers_[name] = value;
  }

  registers_['"'] = value;
  if (operator_ == Operator::Yank) registers_['0'] = value;
}

const Register& VimEngine::register_value(char name) const {
  static const Register kEmpty;
  const auto found = registers_.find(name);
  return found == registers_.end() ? kEmpty : found->second;
}

const Register& VimEngine::active_register() const {
  static const Register kEmpty;
  char name = register_name_;
  if (name >= 'A' && name <= 'Z') name = static_cast<char>(name - 'A' + 'a');
  if (name == '\0' || name == '+' || name == '*') name = '"';
  const auto found = registers_.find(name);
  return found == registers_.end() ? kEmpty : found->second;
}

// --------------------------------------------------------------- normal mode

bool VimEngine::handle_pending_argument(const Key& key) {
  if (awaiting_argument_ == '\0') return false;

  const char waiting = awaiting_argument_;
  awaiting_argument_ = '\0';

  if (key.type == Key::Type::Escape) {
    reset_pending();
    return true;
  }
  if (key.type != Key::Type::Character) {
    reset_pending();
    return true;
  }

  if (waiting == 'r') {
    replace_char(key.text);
    reset_pending();
    return true;
  }
  execute_motion(waiting, key.text);
  return true;
}

bool VimEngine::handle_g_prefix(const Key& key) {
  if (!pending_g_) return false;
  pending_g_ = false;

  switch (key.ascii()) {
    case 'g':
      // `gg` is `G` with a default line of 1, where bare `G` defaults to the
      // last line. Pinning the count here is what distinguishes them.
      if (count_ == 0) count_ = 1;
      execute_motion('G', "");
      return true;
    case 'y':
      yank_result(false);
      reset_pending();
      return true;
    case 'Y':
      yank_result(true);
      reset_pending();
      return true;
    default:
      reset_pending();
      return true;
  }
}

bool VimEngine::handle_z_prefix(const Key& key) {
  if (!pending_z_) return false;
  pending_z_ = false;

  switch (key.ascii()) {
    case 'z': scroll_request_ = ScrollRequest::Center; break;
    case 't': scroll_request_ = ScrollRequest::Top; break;
    case 'b': scroll_request_ = ScrollRequest::Bottom; break;
    default: break;
  }
  reset_pending();
  return true;
}

// Keys that behave identically in normal and visual mode.
bool VimEngine::handle_shared_navigation(const Key& key) {
  switch (key.type) {
    case Key::Type::Left: execute_motion('h', ""); return true;
    case Key::Type::Right: execute_motion('l', ""); return true;
    case Key::Type::Up: execute_motion('k', ""); return true;
    case Key::Type::Down: execute_motion('j', ""); return true;
    case Key::Type::Home: execute_motion('0', ""); return true;
    case Key::Type::End: execute_motion('$', ""); return true;
    case Key::Type::PageDown: scroll_request_ = ScrollRequest::HalfPageDown; return true;
    case Key::Type::PageUp: scroll_request_ = ScrollRequest::HalfPageUp; return true;
    case Key::Type::Control:
      switch (key.text.empty() ? '\0' : key.text[0]) {
        case 'd': scroll_request_ = ScrollRequest::HalfPageDown; return true;
        case 'u': scroll_request_ = ScrollRequest::HalfPageUp; return true;
        case 'e': scroll_request_ = ScrollRequest::LineDown; return true;
        case 'y': scroll_request_ = ScrollRequest::LineUp; return true;
        default: return false;
      }
    default: return false;
  }
}

void VimEngine::feed_normal(const Key& key) {
  if (handle_pending_argument(key)) return;

  if (awaiting_register_) {
    awaiting_register_ = false;
    if (key.type == Key::Type::Character) {
      register_name_ = key.text.size() == 1 ? key.text[0] : '\0';
    }
    return;
  }

  if (handle_g_prefix(key)) return;
  if (handle_z_prefix(key)) return;

  if (key.type == Key::Type::Escape) {
    reset_pending();
    clear_message();
    return;
  }

  if (key.type == Key::Type::Control && key.text == "r") {
    suppress_record_ = true;
    if (!document_.redo()) set_message("already at newest change", false);
    clamp_cursor_for_mode();
    reset_pending();
    return;
  }

  if (handle_shared_navigation(key)) return;

  const char command = key.ascii();
  if (command == '\0') return;

  // A count. A leading zero is the "start of line" motion, not a digit.
  if ((command >= '1' && command <= '9') || (command == '0' && count_ > 0)) {
    count_ = count_ * 10 + (command - '0');
    return;
  }

  if (command == '"') {
    awaiting_register_ = true;
    return;
  }
  if (command == 'g') {
    pending_g_ = true;
    return;
  }
  if (command == 'z') {
    pending_z_ = true;
    return;
  }

  // Operators. A doubled operator key means "this line, linewise".
  if (const Operator candidate = operator_for(command); candidate != Operator::None) {
    if (operator_ != Operator::None) {
      if (command == operator_key_) {
        const int lines = std::max(operator_count_, 1) * effective_count();
        const std::size_t first = document_.cursor().row;
        Range range;
        range.linewise = true;
        range.first_row = first;
        range.row_count = std::min(static_cast<std::size_t>(lines),
                                   document_.line_count() - first);
        apply_operator(range);
        reset_pending();
        return;
      }
      reset_pending();
      return;
    }
    operator_ = candidate;
    operator_key_ = command;
    operator_count_ = count_;
    count_ = 0;
    return;
  }

  if (motion_needs_argument(command)) {
    awaiting_argument_ = command;
    return;
  }
  if (is_motion(command)) {
    execute_motion(command, "");
    return;
  }

  // Anything below is a complete command, so a dangling operator is a mistype.
  if (operator_ != Operator::None) {
    reset_pending();
    return;
  }

  const int count = effective_count();
  switch (command) {
    case 'i':
      enter_insert();
      break;
    case 'I':
      document_.set_cursor(Cursor{document_.cursor().row,
                                  first_non_blank(document_, document_.cursor().row)});
      enter_insert();
      break;
    case 'a': {
      Cursor cursor = document_.cursor();
      if (document_.line_length(cursor.row) > 0) {
        cursor.column = utf8::next_boundary(document_.line(cursor.row), cursor.column);
      }
      document_.set_cursor(cursor);
      enter_insert();
      break;
    }
    case 'A':
      document_.set_cursor(Cursor{document_.cursor().row,
                                  document_.line_length(document_.cursor().row)});
      enter_insert();
      break;
    case 'o': {
      document_.begin_change();
      const std::size_t row = document_.cursor().row;
      document_.insert_lines(row + 1, {std::string()});
      document_.set_cursor(Cursor{row + 1, 0});
      enter_insert();
      break;
    }
    case 'O': {
      document_.begin_change();
      const std::size_t row = document_.cursor().row;
      document_.insert_lines(row, {std::string()});
      document_.set_cursor(Cursor{row, 0});
      enter_insert();
      break;
    }
    case 'x': {
      const Cursor from = document_.cursor();
      MotionResult motion = apply_motion(document_, from, 'l', count, "", true);
      if (from.column >= document_.line_length(from.row)) break;
      operator_ = Operator::Delete;
      apply_operator(resolve_range(document_, from, motion));
      reset_pending();
      break;
    }
    case 'X': {
      const Cursor from = document_.cursor();
      if (from.column == 0) break;
      const MotionResult motion = apply_motion(document_, from, 'h', count, "", true);
      operator_ = Operator::Delete;
      apply_operator(resolve_range(document_, from, motion));
      reset_pending();
      break;
    }
    case 's': {
      const Cursor from = document_.cursor();
      const MotionResult motion = apply_motion(document_, from, 'l', count, "", true);
      operator_ = Operator::Change;
      apply_operator(resolve_range(document_, from, motion));
      reset_pending();
      break;
    }
    case 'D':
    case 'C': {
      const MotionResult motion =
          apply_motion(document_, document_.cursor(), '$', count, "", true);
      operator_ = command == 'D' ? Operator::Delete : Operator::Change;
      apply_operator(resolve_range(document_, document_.cursor(), motion));
      reset_pending();
      break;
    }
    case 'S':
    case 'Y': {
      Range range;
      range.linewise = true;
      range.first_row = document_.cursor().row;
      range.row_count = std::min(static_cast<std::size_t>(count),
                                 document_.line_count() - range.first_row);
      operator_ = command == 'S' ? Operator::Change : Operator::Yank;
      apply_operator(range);
      reset_pending();
      break;
    }
    case 'p':
      put(true);
      reset_pending();
      break;
    case 'P':
      put(false);
      reset_pending();
      break;
    case 'u':
      suppress_record_ = true;
      if (!document_.undo()) set_message("already at oldest change", false);
      clamp_cursor_for_mode();
      reset_pending();
      break;
    case 'r':
      awaiting_argument_ = 'r';
      break;
    case '~': {
      const Cursor from = document_.cursor();
      const MotionResult motion = apply_motion(document_, from, 'l', count, "", true);
      document_.begin_change();
      toggle_case_range(from, motion.target);
      document_.set_cursor(document_.clamped(motion.target, false));
      document_.commit_change();
      reset_pending();
      break;
    }
    case 'J':
      join_lines_command();
      reset_pending();
      break;
    case 'v':
      enter_visual(Mode::Visual);
      reset_pending();
      break;
    case 'V':
      enter_visual(Mode::VisualLine);
      reset_pending();
      break;
    case '.':
      repeat_last_change();
      break;
    case ':':
    case '/':
    case '?':
      command_line_ = std::string(1, command);
      mode_ = Mode::CommandLine;
      clear_message();
      reset_pending();
      break;
    case 'n':
      search_next(last_search_forward_);
      reset_pending();
      break;
    case 'N':
      search_next(!last_search_forward_);
      reset_pending();
      break;
    default:
      reset_pending();
      break;
  }
}

// -------------------------------------------------------------- motion + ops

void VimEngine::execute_motion(char key, const std::string& argument) {
  int count = count_;
  if (operator_ != Operator::None && operator_count_ > 0) {
    // vim multiplies the two counts: 2d3w deletes six words.
    count = (count > 0 ? count : 1) * operator_count_;
  }
  // A count on G names a line rather than a repetition, so it must not be
  // multiplied by the operator count.
  if (key == 'G') count = count_;

  const Cursor start = document_.cursor();

  // vim's own quirk: on a non-blank, `cw` changes to the end of the current word
  // rather than moving like `w` would. Note this is not simply `ce` — on a
  // one-character word such as the "1" in "1 + 2", `ce` would run on to the end
  // of the next word while `cw` must touch only that character.
  if (operator_ == Operator::Change && (key == 'w' || key == 'W')) {
    const char under = char_under_cursor();
    if (under != '\0' && under != ' ' && under != '\t') {
      MotionResult word;
      word.valid = true;
      word.kind = MotionKind::CharwiseInclusive;
      word.target = end_of_current_word(document_, start, count, key == 'W');
      apply_operator(resolve_range(document_, start, word));
      reset_pending();
      return;
    }
  }

  const MotionResult motion = apply_motion(document_, start, key, count, argument,
                                           operator_ != Operator::None);
  if (!motion.valid) {
    reset_pending();
    return;
  }

  if (operator_ == Operator::None) {
    move_cursor(motion, key);
    reset_pending();
    return;
  }

  apply_operator(resolve_range(document_, start, motion));
  reset_pending();
}

void VimEngine::move_cursor(const MotionResult& motion, char key) {
  Cursor target = motion.target;

  // j and k keep the column the user last aimed at, so moving through a short
  // line and out the other side returns to the original column.
  if (key == 'j' || key == 'k') target.column = desired_column_;

  document_.set_cursor(document_.clamped(target, allows_cursor_past_end(mode_)));

  if (key == '$') {
    // `$` sticks to the end of the line across subsequent j and k.
    desired_column_ = static_cast<std::size_t>(-1);
  } else if (key != 'j' && key != 'k') {
    desired_column_ = document_.cursor().column;
  }
}

void VimEngine::apply_operator(const Range& range) {
  Register captured;
  if (range.linewise) {
    captured.linewise = true;
    captured.text = document_.lines_text(range.first_row, range.row_count);
  } else {
    captured.text = document_.text_range(range.from, range.to);
  }

  switch (operator_) {
    case Operator::Yank: {
      store_register(captured);
      if (range.linewise) {
        document_.set_cursor(Cursor{range.first_row, document_.cursor().column});
      } else {
        document_.set_cursor(range.from);
      }
      clamp_cursor_for_mode();
      break;
    }
    case Operator::Delete: {
      document_.begin_change();
      store_register(captured);
      if (range.linewise) {
        document_.erase_lines(range.first_row, range.row_count);
        const std::size_t row = std::min(range.first_row, document_.last_row());
        document_.set_cursor(Cursor{row, first_non_blank(document_, row)});
      } else {
        document_.erase_range(range.from, range.to);
        document_.set_cursor(range.from);
      }
      document_.commit_change();
      clamp_cursor_for_mode();
      break;
    }
    case Operator::Change: {
      document_.begin_change();
      store_register(captured);
      if (range.linewise) {
        // `cc` blanks the lines but keeps the indent and leaves one line to
        // type on. Replacing then trimming avoids the "deleted the only line"
        // edge case an erase-then-insert would hit.
        const std::size_t indent = first_non_blank(document_, range.first_row);
        std::string prefix = document_.line(range.first_row).substr(0, indent);
        const std::size_t prefix_size = prefix.size();
        document_.replace_line(range.first_row, std::move(prefix));
        if (range.row_count > 1) {
          document_.erase_lines(range.first_row + 1, range.row_count - 1);
        }
        document_.set_cursor(Cursor{range.first_row, prefix_size});
      } else {
        document_.erase_range(range.from, range.to);
        document_.set_cursor(range.from);
      }
      enter_insert();
      break;
    }
    case Operator::None:
      break;
  }
}

// ----------------------------------------------------------------- mode moves

void VimEngine::enter_insert() {
  // Clearing here rather than at each call site keeps a stray count (`3i`) from
  // leaking into insert mode, where it would make has_pending() stick and stop
  // `.` from ever recording the change.
  reset_pending();
  document_.begin_change();
  mode_ = Mode::Insert;
  clear_message();
}

void VimEngine::leave_insert() {
  document_.commit_change();
  mode_ = Mode::Normal;
  // vim steps the cursor back off the position past the last character.
  Cursor cursor = document_.cursor();
  if (cursor.column > 0) {
    cursor.column = utf8::prev_boundary(document_.line(cursor.row), cursor.column);
  }
  document_.set_cursor(document_.clamped(cursor, false));
  desired_column_ = document_.cursor().column;
}

void VimEngine::enter_visual(Mode visual_mode) {
  reset_pending();
  mode_ = visual_mode;
  visual_anchor_ = document_.cursor();
}

// --------------------------------------------------------------- insert mode

void VimEngine::feed_insert(const Key& key) {
  switch (key.type) {
    case Key::Type::Escape:
      leave_insert();
      return;
    case Key::Type::Enter: {
      const Cursor cursor = document_.cursor();
      document_.split_line(cursor);
      document_.set_cursor(Cursor{cursor.row + 1, 0});
      return;
    }
    case Key::Type::Backspace: {
      const Cursor cursor = document_.cursor();
      if (cursor.column > 0) {
        const std::size_t previous =
            utf8::prev_boundary(document_.line(cursor.row), cursor.column);
        document_.erase_range(Cursor{cursor.row, previous}, cursor);
        document_.set_cursor(Cursor{cursor.row, previous});
      } else if (cursor.row > 0) {
        const std::size_t join_column = document_.line_length(cursor.row - 1);
        document_.join_lines(cursor.row - 1);
        document_.set_cursor(Cursor{cursor.row - 1, join_column});
      }
      return;
    }
    case Key::Type::Delete: {
      const Cursor cursor = document_.cursor();
      if (cursor.column < document_.line_length(cursor.row)) {
        const std::size_t next =
            utf8::next_boundary(document_.line(cursor.row), cursor.column);
        document_.erase_range(cursor, Cursor{cursor.row, next});
      } else if (cursor.row < document_.last_row()) {
        document_.join_lines(cursor.row);
      }
      return;
    }
    case Key::Type::Tab:
      // A scratchpad wants spaces, never a literal tab in an expression.
      document_.insert_text(document_.cursor(), "  ");
      document_.set_cursor(Cursor{document_.cursor().row, document_.cursor().column + 2});
      return;
    case Key::Type::Character: {
      const Cursor cursor = document_.cursor();
      document_.insert_text(cursor, key.text);
      document_.set_cursor(Cursor{cursor.row, cursor.column + key.text.size()});
      return;
    }
    case Key::Type::Left:
    case Key::Type::Right:
    case Key::Type::Up:
    case Key::Type::Down:
    case Key::Type::Home:
    case Key::Type::End:
      handle_shared_navigation(key);
      return;
    default:
      return;
  }
}

// --------------------------------------------------------------- visual mode

void VimEngine::feed_visual(const Key& key) {
  if (handle_pending_argument(key)) return;

  if (awaiting_register_) {
    awaiting_register_ = false;
    if (key.type == Key::Type::Character && key.text.size() == 1) {
      register_name_ = key.text[0];
    }
    return;
  }
  if (handle_g_prefix(key)) return;
  if (handle_z_prefix(key)) return;

  if (key.type == Key::Type::Escape) {
    mode_ = Mode::Normal;
    reset_pending();
    clamp_cursor_for_mode();
    return;
  }
  if (handle_shared_navigation(key)) return;

  const char command = key.ascii();
  if (command == '\0') return;

  if ((command >= '1' && command <= '9') || (command == '0' && count_ > 0)) {
    count_ = count_ * 10 + (command - '0');
    return;
  }

  switch (command) {
    case 'v':
      mode_ = mode_ == Mode::Visual ? Mode::Normal : Mode::Visual;
      reset_pending();
      clamp_cursor_for_mode();
      return;
    case 'V':
      mode_ = mode_ == Mode::VisualLine ? Mode::Normal : Mode::VisualLine;
      reset_pending();
      clamp_cursor_for_mode();
      return;
    case 'o': {
      // Swap which end of the selection the cursor controls.
      const Cursor cursor = document_.cursor();
      document_.set_cursor(visual_anchor_);
      visual_anchor_ = cursor;
      reset_pending();
      return;
    }
    case '"':
      awaiting_register_ = true;
      return;
    case 'g':
      pending_g_ = true;
      return;
    case 'z':
      pending_z_ = true;
      return;
    case 'd':
    case 'x':
      apply_visual_operator(Operator::Delete);
      return;
    case 'y':
      apply_visual_operator(Operator::Yank);
      return;
    case 'c':
    case 's':
      apply_visual_operator(Operator::Change);
      return;
    case '~': {
      const auto span = selection();
      if (!span) return;
      document_.begin_change();
      Cursor to = span->second;
      to.column = utf8::next_boundary(document_.line(to.row), to.column);
      toggle_case_range(span->first, to);
      document_.commit_change();
      mode_ = Mode::Normal;
      document_.set_cursor(span->first);
      reset_pending();
      clamp_cursor_for_mode();
      return;
    }
    case ':':
      command_line_ = ":";
      mode_ = Mode::CommandLine;
      reset_pending();
      return;
    default:
      break;
  }

  if (motion_needs_argument(command)) {
    awaiting_argument_ = command;
    return;
  }
  if (is_motion(command)) {
    execute_motion(command, "");
    return;
  }
  reset_pending();
}

void VimEngine::apply_visual_operator(Operator op) {
  const auto span = selection();
  if (!span) return;

  Range range;
  if (mode_ == Mode::VisualLine) {
    range.linewise = true;
    range.first_row = span->first.row;
    range.row_count = span->second.row - span->first.row + 1;
  } else {
    range.from = span->first;
    range.to = span->second;
    // A visual selection includes the character under the cursor.
    range.to.column = utf8::next_boundary(document_.line(range.to.row), range.to.column);
  }

  mode_ = Mode::Normal;
  operator_ = op;
  apply_operator(range);
  reset_pending();
}

// -------------------------------------------------------- command-line mode

void VimEngine::feed_command_line(const Key& key) {
  switch (key.type) {
    case Key::Type::Escape:
      command_line_.clear();
      mode_ = Mode::Normal;
      return;
    case Key::Type::Enter: {
      const std::string text = command_line_;
      command_line_.clear();
      mode_ = Mode::Normal;
      if (text.size() < 2) return;
      if (text[0] == ':') {
        run_ex(text.substr(1));
      } else {
        run_search(text.substr(1), text[0] == '/');
      }
      return;
    }
    case Key::Type::Backspace:
      if (command_line_.size() <= 1) {
        command_line_.clear();
        mode_ = Mode::Normal;
        return;
      }
      command_line_.erase(utf8::prev_boundary(command_line_, command_line_.size()));
      return;
    case Key::Type::Character:
      command_line_ += key.text;
      return;
    default:
      return;
  }
}

void VimEngine::run_ex(const std::string& command) {
  const ExOutcome outcome = execute_ex_command(command, document_);

  if (outcome.line_numbers.has_value()) line_numbers_ = *outcome.line_numbers;
  if (outcome.goto_row.has_value()) {
    const std::size_t row = std::min(*outcome.goto_row, document_.last_row());
    document_.set_cursor(Cursor{row, first_non_blank(document_, row)});
    clamp_cursor_for_mode();
  }
  if (outcome.open_url.has_value() && url_opener_) url_opener_(*outcome.open_url);
  if (outcome.quit) quit_requested_ = true;

  // A reload through :e replaces the buffer, so the cursor may be stale.
  clamp_cursor_for_mode();
  if (!outcome.message.empty() || outcome.is_error) {
    set_message(outcome.message, outcome.is_error);
  } else {
    clear_message();
  }
}

// -------------------------------------------------------------------- search

void VimEngine::run_search(const std::string& pattern, bool forward) {
  if (!pattern.empty()) {
    last_search_ = pattern;
    last_search_forward_ = forward;
  }
  search_next(forward);
}

bool VimEngine::search_next(bool forward) {
  if (last_search_.empty()) {
    set_message("no previous search", true);
    return false;
  }

  const std::size_t rows = document_.line_count();
  const Cursor start = document_.cursor();

  // Walk the buffer from just past the cursor, wrapping once.
  for (std::size_t step = 0; step <= rows; ++step) {
    const std::size_t row = forward ? (start.row + step) % rows
                                    : (start.row + rows - step % rows) % rows;
    const std::string& text = document_.line(row);
    std::size_t found = std::string::npos;

    if (forward) {
      const std::size_t from = step == 0 ? start.column + 1 : 0;
      if (from <= text.size()) found = text.find(last_search_, from);
    } else {
      if (step == 0) {
        found = start.column == 0 ? std::string::npos
                                  : text.rfind(last_search_, start.column - 1);
      } else {
        found = text.rfind(last_search_);
      }
    }

    if (found != std::string::npos) {
      document_.set_cursor(Cursor{row, found});
      clamp_cursor_for_mode();
      desired_column_ = found;
      clear_message();
      return true;
    }
  }

  set_message("pattern not found: " + last_search_, true);
  return false;
}

// ------------------------------------------------------------------ commands

void VimEngine::put(bool after) {
  const Register value = active_register();
  if (value.text.empty()) return;

  const int count = effective_count();
  document_.begin_change();

  if (value.linewise) {
    std::vector<std::string> inserted;
    for (int step = 0; step < count; ++step) {
      const std::vector<std::string> once = split_lines(value.text);
      inserted.insert(inserted.end(), once.begin(), once.end());
    }
    const std::size_t row = after ? document_.cursor().row + 1 : document_.cursor().row;
    document_.insert_lines(row, inserted);
    document_.set_cursor(Cursor{row, first_non_blank(document_, row)});
  } else {
    Cursor at = document_.cursor();
    if (after && document_.line_length(at.row) > 0) {
      at.column = utf8::next_boundary(document_.line(at.row), at.column);
    }
    const Cursor end = document_.insert_text_multiline(at, repeat(value.text, count));
    // vim leaves the cursor on the last character that was put.
    Cursor cursor = end;
    if (cursor.column > 0) {
      cursor.column = utf8::prev_boundary(document_.line(cursor.row), cursor.column);
    }
    document_.set_cursor(cursor);
  }

  document_.commit_change();
  clamp_cursor_for_mode();
}

void VimEngine::replace_char(const std::string& replacement) {
  const Cursor cursor = document_.cursor();
  const int count = effective_count();
  const std::string& text = document_.line(cursor.row);

  // `3ra` needs three characters to overwrite, or it does nothing at all.
  std::size_t end = cursor.column;
  for (int step = 0; step < count; ++step) {
    if (end >= text.size()) return;
    end = utf8::next_boundary(text, end);
  }

  document_.begin_change();
  document_.erase_range(cursor, Cursor{cursor.row, end});
  document_.insert_text(cursor, repeat(replacement, count));
  document_.set_cursor(
      Cursor{cursor.row, cursor.column + replacement.size() * static_cast<std::size_t>(count)});
  document_.commit_change();
  // The cursor rests on the last replaced character.
  Cursor rest = document_.cursor();
  if (rest.column > 0) {
    rest.column = utf8::prev_boundary(document_.line(rest.row), rest.column);
  }
  document_.set_cursor(rest);
  clamp_cursor_for_mode();
}

void VimEngine::toggle_case_range(Cursor from, Cursor to) {
  if (to < from) std::swap(from, to);
  for (std::size_t row = from.row; row <= to.row && row < document_.line_count(); ++row) {
    const std::size_t begin = row == from.row ? from.column : 0;
    const std::size_t end = row == to.row ? std::min(to.column, document_.line_length(row))
                                        : document_.line_length(row);
    if (begin >= end) continue;
    std::string text = document_.line(row);
    const std::string flipped = toggle_case(text.substr(begin, end - begin));
    text.replace(begin, end - begin, flipped);
    document_.replace_line(row, std::move(text));
  }
}

void VimEngine::join_lines_command() {
  // `J` joins the next line; `3J` joins three lines into one.
  const int joins = std::max(effective_count() - 1, 1);
  const std::size_t row = document_.cursor().row;

  document_.begin_change();
  for (int step = 0; step < joins; ++step) {
    if (row >= document_.last_row()) break;
    std::string next = document_.line(row + 1);
    const std::size_t first = next.find_first_not_of(" \t");
    next = first == std::string::npos ? std::string() : next.substr(first);

    std::string current = document_.line(row);
    // The cursor lands where the two lines met.
    const std::size_t join_column = current.size();
    // vim inserts a single space at the join unless the line already ends in one.
    if (!current.empty() && !next.empty() && current.back() != ' ') current += ' ';
    current += next;

    document_.replace_line(row, std::move(current));
    document_.erase_lines(row + 1, 1);
    document_.set_cursor(Cursor{row, join_column});
  }
  document_.commit_change();
  clamp_cursor_for_mode();
}

void VimEngine::yank_result(bool with_expression) {
  const std::size_t row = document_.cursor().row;
  const LineEval& eval = results_.at(row);
  if (!eval.has_result()) {
    set_message("no result on this line", true);
    return;
  }

  Register value;
  if (!with_expression) {
    // `gy` yanks the value even where it is not displayed, so the number behind
    // `x = 5` is still reachable.
    value.text = eval.text;
  } else if (eval.show_result) {
    value.text = document_.line(row) + " = " + eval.text;
  } else {
    // The line already reads as `name = value`; appending would give
    // "subtotal = 128.40 = 128.4".
    value.text = document_.line(row);
  }
  registers_['"'] = value;
  registers_['0'] = value;
  if (clipboard_writer_) clipboard_writer_(value.text);
  set_message("yanked " + value.text, false);
}

void VimEngine::repeat_last_change() {
  if (last_change_.empty()) {
    reset_pending();
    return;
  }
  reset_pending();
  const std::vector<Key> keys = last_change_;
  replaying_ = true;
  for (const Key& key : keys) feed(key);
  replaying_ = false;
  // Set after the replay, not before: each nested feed() clears this flag, and
  // `.` must never overwrite the change it just repeated.
  suppress_record_ = true;
}

}  // namespace calc
