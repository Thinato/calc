# calc

A vim-modal calculator scratchpad for the terminal. Every line is an expression;
its result is shown after it and cannot be edited.

```
 1 1 + 2 = 3
 2 sqrt(16) + pow(2, 10) = 1028
 3 (1 + 2) * 3 ^ 2 = 27
 4
~
────────────────────────────────────────────────────────────
 NORMAL  notes.calc [+]                                 4:1
```

It behaves like a text editor rather than a prompt: move around, edit any line,
and every result updates as you type.

## Build

Needs CMake 3.24+ and a C++20 compiler. FTXUI and doctest are fetched
automatically, so a fresh clone needs nothing installed.

```sh
cmake --preset default
cmake --build build
ctest --test-dir build          # 124 tests, no terminal required
./build/calc                    # scratch buffer
./build/calc notes.calc         # open a file
```

## The language

| | |
| --- | --- |
| `+` `-` `*` `/` | add, subtract, multiply, divide |
| `^` | power, right associative: `2^3^2` is `512` |
| `(` `)` | grouping |
| `pow(a, b)` | power |
| `sqrt(a)` | square root |
| `#` | comment to the end of the line |

Precedence is the ordinary mathematical one, so `1 + 2 * 3` is `7` and `-2^2` is
`-4`. A negative exponent needs no parentheses: `2^-1` is `0.5`.

Results are formatted the way a calculator should: `3` rather than `3.000000`,
and `0.1 + 0.2` reads `0.3` rather than `0.30000000000000004`.

A line that does not parse simply shows no result — a half-typed expression is
the normal state while typing. The reason appears in the bottom line when the
cursor is on that line, with the column it went wrong at.

## Keys

**Motions** `h j k l` · `0 ^ $` · `w W b B e E` · `gg G` · `f F t T` · `%` ·
`{ }` · `|` · arrow keys, Home, End

**Operators** `d` `c` `y` compose with every motion above, with counts:
`dw` `d$` `d2w` `2d3w` `df(` `d%` · doubled for whole lines: `dd` `cc` `yy` ·
shorthands `D` `C` `S` `Y` `x` `X` `s`

**Edits** `i a I A o O` · `r` · `~` · `J` · `p P` · `u` and `Ctrl-R` · `.` repeats
the last change, including a whole insert session

**Visual** `v` `V`, then `d c y x ~`, `o` to swap ends

**Search** `/` `?` then `n` `N`

**Registers** `"a`–`"z` (uppercase appends), `"0` holds the last yank, `"+`
goes to the system clipboard

**Scrolling** `Ctrl-D` `Ctrl-U` `Ctrl-E` `Ctrl-Y` · `zz` `zt` `zb`

**Calculator extras**

| | |
| --- | --- |
| `gy` | yank this line's result to the clipboard |
| `gY` | yank `expression = result` |

**Commands** `:w [file]` · `:wq` `:x` · `:q` `:q!` · `:e[!] file` ·
`:42` jumps to a line · `:set number` / `:set nonumber` · `:help`

## Results are not text

The result is not stored in the buffer. A line holds only what you typed;
results live in a separate cache and are drawn by the renderer. The cursor is
clamped to the end of the typed text, so no motion, operator or paste can reach
the result column — it is unreachable by construction rather than guarded by
checks.

Two things follow from that. `$` and `A` land at the end of your expression, not
after the result. And `:w` writes only the expressions, so a saved file stays
clean and reopens unchanged.

`tests/test_invariants.cpp` enforces this by feeding the engine random key
sequences and asserting the cursor never passes the end of a line and no `=`
ever appears in the buffer.

## Layout

```
src/core/    expression language: lexer, parser, eval, number formatting
src/doc/     the buffer, cursor, undo, result cache, file IO
src/vim/     the modal editor: motions, operators, state machine, ex commands
src/ui/      FTXUI rendering and the event loop
```

`core`, `doc` and `vim` link only the standard library, so all of the program's
logic is testable without a terminal. FTXUI appears in `src/ui/` alone.

The editor is one idea: operators compose with motions. `dd` is not a special
case, it is operator `d` over a linewise current-line motion, which is why most
new commands are a table entry in `src/vim/motions.cpp` rather than a branch in
the state machine.
