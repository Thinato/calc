# calc

A vim-modal calculator scratchpad for the terminal. Every line is an expression;
its result is shown after it and cannot be edited.

```
 1 1 + 2 = 3
 2 sqrt(16) + pow(2, 10) = 1028
 3 subtotal = 128.40
 4 subtotal * 0.0825 = 10.593
 5 subtotal / 0  Error: division by zero
 6
~
────────────────────────────────────────────────────────────
 NORMAL  notes.calc [+]                                 6:1
```

It behaves like a text editor rather than a prompt: move around, edit any line,
name values and reuse them, and every result updates as you type.

## Build

Needs CMake 3.24+ and a C++20 compiler. FTXUI and doctest are fetched
automatically, so a fresh clone needs nothing installed.

```sh
cmake --preset default
cmake --build build
ctest --test-dir build          # 190 tests, no terminal required
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
| `name = expr` | define a variable or constant |
| `#` | comment to the end of the line |

Precedence is the ordinary mathematical one, so `1 + 2 * 3` is `7` and `-2^2` is
`-4`. A negative exponent needs no parentheses: `2^-1` is `0.5`.

Results are formatted the way a calculator should: `3` rather than `3.000000`,
and `0.1 + 0.2` reads `0.3` rather than `0.30000000000000004`.

## Colour

Every colour answers one question, so the screen can be read at a glance:

| | |
| --- | --- |
| dimmed | a comment — prose to skim past |
| light blue | a function: `sqrt`, `pow` |
| yellow | a variable, where it is defined |
| magenta | a constant, where it is defined |
| cyan | a result |
| red | an error |

A name is light blue only when it really is a function, so `foo(2)` stays plain
and agrees with the `unknown function` beside it. Only the line that *defines* a
name colours it; later uses of `subtotal` are left alone.

On a terminal without 256-colour support each shade falls back to the nearest of
the basic sixteen.

## Errors

A line that does not work says why, in red, where its result would have gone:

```
 1 1 / 0  Error: division by zero
 2 nope * 2  Error: undefined name 'nope'
```

With one exception: **the line the cursor is on stays quiet.** A half-typed
expression is the normal state while typing, so being told about it on every
keystroke would only get in the way. Move off the line and the reason appears.

## Variables and constants

Name a value and reuse it below:

```
 1 subtotal = 128.40
 2 RATE = 0.0825
 3 tip = subtotal * 0.2 = 25.68
 4 subtotal * RATE = 10.593
 5 subtotal + tip = 154.08
```

Which kind of name you get is decided by spelling alone:

| | |
| --- | --- |
| **variable** | holds at least one lowercase letter — `x`, `test`, `helloWorld`, `xOne`. Reassign it freely. |
| **constant** | holds no lowercase letters — `PI`, `RATE`, `TEST_ONE`. Defining it twice is an error that names the line it came from. |

`PI`, `E` and `TAU` are built in, and protected by the same rule, so `PI = 3` is
an error rather than a silent redefinition.

Names are **letters and underscores only**. `x1`, `1_x` and `x.y` are rejected,
and the message quotes what you typed rather than mis-reading it as something
else:

```
x1 = 5  Error: invalid name 'x1': names cannot contain digits
```

Names resolve **top to bottom**, like reading a script. A name works only below
the line that defines it, so using one earlier is an `undefined name` error — and
`x = x + 1` therefore reads as an increment. Editing a definition recomputes
every line under it as you type.

A definition shows its computed value unless you typed the value out literally:
`x = 1 + 2` gains `= 3`, while `subtotal = 128.40` shows nothing extra, because a
number already on the line does not need restating (and `128.40` should not
visibly become `128.4` beside itself). `gy` still yanks the value either way.

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

Neither the result nor the error is stored in the buffer. A line holds only what
you typed; both overlays live in a separate cache and are drawn by the renderer.
The cursor is clamped to the end of the typed text, so no motion, operator or
paste can reach the result column — it is unreachable by construction rather than
guarded by checks.

Two things follow from that. `$` and `A` land at the end of your expression, not
after the result. And `:w` writes only the expressions, so a saved file stays
clean and reopens unchanged.

`tests/test_invariants.cpp` enforces this by feeding the engine random key
sequences and asserting the cursor never passes the end of a line, and that no
line ever ends with the result — or the error — rendered for it. (Note it cannot
simply ban `=` from the buffer: since assignments exist, `=` is legitimate text
you type.)

## Layout

```
src/core/    expression language: lexer, parser, eval, environment, formatting
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
