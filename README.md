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

**[Try it in the browser](https://lisecki.dev/calc/)** — the same program compiled
to WebAssembly, no install.

## Install

```sh
curl --proto '=https' --tlsv1.2 -sSf \
  https://raw.githubusercontent.com/Thinato/calc/main/install.sh | sh
```

The binary lands in `~/.local/bin`. `CALC_INSTALL_DIR` changes where it goes and
`CALC_VERSION` picks a release other than the latest. Published builds cover Linux
x86_64 and arm64 (glibc 2.35 or newer) and macOS on Apple Silicon and Intel;
anything else builds from source in a few seconds.

## Build

Needs CMake 3.24+ and a C++20 compiler. FTXUI and doctest are fetched
automatically, so a fresh clone needs nothing installed.

```sh
cmake --preset default
cmake --build build
ctest --test-dir build          # 253 tests, no terminal required
./build/calc                    # scratch buffer
./build/calc notes.calc         # open a file
```

### For the web

`web/` holds a second entry point: the same core, driven one animation frame at a
time instead of by a blocking loop, with the clipboard and `:github` going through
the browser. It needs Emscripten (`brew install emscripten`):

```sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-web
mkdir -p site && cp web/index.html web/calc.css web/calc.mjs build-web/calc.js \
  build-web/calc.wasm site/
python3 -m http.server 8888 --directory site
```

The build is deliberately single-threaded, which is what lets it be embedded in a
page: a threaded build needs `SharedArrayBuffer`, and an iframe only gets that if
the page around it is cross-origin isolated too. `web/main.cpp` says the rest.

On the web `:w` and `:e` work against a filesystem that lives in the tab, so a
save is gone on reload, and `:q` restarts on the demo buffer rather than leaving a
dead terminal in the page.

## Checks

CI runs these on every push, and they are the same four commands locally. Both
clang tools are pinned, because their output changes between major versions:

```sh
pip install clang-format==22.1.8 clang-tidy==22.1.8

# layout, then defects (-p build reads the compile database the preset exports)
find src tests web \( -name '*.cpp' -o -name '*.hpp' \) | xargs clang-format -i
find src tests -name '*.cpp' | xargs clang-tidy -p build

# the suite twice: warnings as errors, then again under ASan and UBSan
cmake --preset default -DCALC_WERROR=ON && cmake --build build && ctest --preset default
cmake --preset debug && cmake --build build-debug && ctest --preset debug
```

`.clang-tidy` keeps only the check families that look for defects — layout is
`clang-format`'s job, and the `readability` and `modernize` families disagree with
this codebase on purpose. Two directories narrow it further: `tests/` for a check
that cannot see through a doctest guard, and `src/ui/` for the two that flag
leaving the process, which is what the clipboard and `:github` do. All three
config files say why, check by check.

CI runs one more job than there are commands here: the WebAssembly build, so a
change that only breaks the web port still fails the push. `web/` is formatted but
not linted, because `clang-tidy -p build` reads the native compile database and
that entry point is only ever compiled by Emscripten.

## The language

| symbol | meaning |
| --- | --- |
| `+` `-` `*` `/` | add, subtract, multiply, divide |
| `^` | power, right associative: `2^3^2` is `512` |
| `(` `)` | grouping |
| `pow(a, b)` | power |
| `sqrt(a)` | square root |
| `name = expr` | define a variable or constant |
| `define f(x): expr` | define a function |
| `define f(x) { … }` | the same, with a body of several statements |
| `#` | comment to the end of the line |

Precedence is the ordinary mathematical one, so `1 + 2 * 3` is `7` and `-2^2` is
`-4`. A negative exponent needs no parentheses: `2^-1` is `0.5`.

Results are formatted the way a calculator should: `3` rather than `3.000000`,
and `0.1 + 0.2` reads `0.3` rather than `0.30000000000000004`.

## Colour

Every colour answers one question, so the screen can be read at a glance:

| color | meaning |
| --- | --- |
| dimmed | a comment — prose to skim past |
| light blue | a function: `sqrt`, `pow`, and the ones you define |
| yellow | a variable, where it is defined |
| magenta | a constant, where it is defined |
| cyan | a result, and the curve `:plot` draws out of one |
| red | an error |
| bold | `define` and `return` — structure, not a value |

A name is light blue only when it really is a function, so `foo(2)` stays plain
and agrees with the `unknown function` beside it. A function you define is light
blue from the line that defines it downward, and a call written above that line
stays plain — the colour and the error never disagree. Only the line that
*defines* a name colours it; later uses of `subtotal` are left alone.

Structure is bold rather than coloured, because each colour above answers a
question about the value on the line, and `define` is not one of them.

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

| type | meaning |
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

`define` and `return` are reserved words, so they are the two spellings a name
cannot have.

Names resolve **top to bottom**, like reading a script. A name works only below
the line that defines it, so using one earlier is an `undefined name` error — and
`x = x + 1` therefore reads as an increment. Editing a definition recomputes
every line under it as you type.

A definition shows its computed value unless you typed the value out literally:
`x = 1 + 2` gains `= 3`, while `subtotal = 128.40` shows nothing extra, because a
number already on the line does not need restating (and `128.40` should not
visibly become `128.4` beside itself). `gy` still yanks the value either way.

## Functions

Name a computation the way you name a value. Three spellings, one grammar:

```
 1 define double(x): x * 2
 2 double(21) = 42
 3
 4 define hyp(a, b) { squares = a^2 + b^2; return sqrt(squares) }
 5 hyp(3, 4) = 5
 6
 7 define area(r) {
 8   PI * r^2
 9 }
10 area(3) = 28.2743338823
```

The body after `:` is the rest of the line. A `{ }` body is the same thing with
room to breathe: statements separated by `;`, or by a row break, which means the
same. **Its value is the last statement**, so `return` is optional — and since an
assignment already has a value here, `{ z = x * 2 }` is a body worth `2x`.

A body reads the names around it **where it is called**, with the parameters
shadowing them:

```
 1 define with_tax(amount): amount * (1 + RATE)
 2 RATE = 0.0825
 3 with_tax(100) = 108.25
```

Two things follow from that. A body's mistake only shows up when something calls
it, so the error appears on the calling line, named after the function that
failed: `f(1)  Error: in f(): undefined name 'nope'`. And a body may only call
functions that already exist above it, so a function cannot call itself — one
that comes to anyway, through a redefinition, is stopped with `too much recursion`
rather than taking the program down with it.

Otherwise functions follow every rule names already follow. They resolve top to
bottom, so a call above the definition is an `unknown function`. A lowercase name
can be redefined and an ALLCAPS one cannot, by the same spelling rule as values.
`sqrt` and `PI` are not available to take. And a definition is not a value, so
nothing is drawn after it.

While a `{` has no `}` yet, there is no block: that line says `unclosed '{'` and
every line under it is still evaluated as itself, so opening a brace at the top
of a long scratchpad never blanks the results below it.

## Plotting

`:plot` draws the function the cursor is on, in braille, in a panel below the
buffer:

```
 1 define bell(x): E ^ -(x ^ 2)
~
──────────────────────────────────────────────────────────────
 bell  x -3..3  y 0.000123..1
       1                       ⣀⠴⠚⠉⠍⠙⠲⢄⡀
                             ⣠⠞⠁   ⠅   ⠙⢦⡀
                          ⢀⡤⠊      ⠅     ⠈⠢⣄
                       ⢀⣠⠔⠋        ⠅       ⠈⠓⢤⣀
0.000123 ⣀⣀⣀⣀⣀⣀⣀⣀⣀⣠⡤⡤⡖⡚⡉⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡅⡀⡀⡀⡀⡀⡀⡀⡀⡀⡀⡈⡙⡒⡦⡤⣤⣀⣀⣀⣀⣀⣀⣀⣀⣀⣀
         -3                                                  3
```

The panel **stays and redraws as you type**, like every other result here, so
editing the body animates the curve. `:noplot` closes it.

Two dimensions means one parameter: `hyp(a, b)` is refused. `:plot` finds what
to draw in the order you would point at it — the definition the cursor is in
(the `define` row or any row of its body), else the first one-parameter function
named on the cursor line, so it works on a `bell(1)` line too. `:plot sqrt`
names one outright, builtins included.

x runs `-10..10` unless you say otherwise, and y is scaled to fit what was
found:

| command | what it draws |
| --- | --- |
| `:plot` | the function under the cursor, x `-10..10` |
| `:plot bell` | that one instead |
| `:plot bell -3..3` | a chosen x range |
| `:plot bell -3..3 0..1` | a chosen y range too, instead of scaling to fit |

The curve is cyan because it *is* a result; the axes are dim, and the labels
give the corners of what you are looking at. Where the function has no value the
curve breaks rather than joining across the gap, so `1 / x` shows two branches
instead of a line through the pole — though scaling to fit a pole squashes
everything else flat, which is what the y range argument is for.

A body's error surfaces here the way it does on a calling line: the panel says
`in f(): undefined name 'nope'` and draws nothing. Delete the definition while
the panel is open and it says `unknown function`, then draws again when you type
it back.

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

| keybind | what it does |
| --- | --- |
| `gy` | yank this line's result to the clipboard |
| `gY` | yank `expression = result` |

**Commands** `:w [file]` · `:wq` `:x` · `:q` `:q!` · `:e[!] file` ·
`:42` jumps to a line · `:set number` / `:set nonumber` ·
`:plot [name] [x] [y]` charts a function · `:noplot` closes it · `:help` ·
`:github` opens [the project page](https://github.com/Thinato/calc) in a browser

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
src/core/    expression language: lexer, parser, eval, environment, formatting,
             and the sampling behind :plot — the drawing of it lives in src/ui/
src/doc/     the buffer, cursor, undo, result cache, file IO
src/vim/     the modal editor: motions, operators, state machine, ex commands
src/ui/      FTXUI rendering, the session, and the terminal's event loop
web/         the WebAssembly entry point and the page that hosts it
```

`core`, `doc` and `vim` link only the standard library, so all of the program's
logic is testable without a terminal. FTXUI appears in `src/ui/` alone.

The editor is one idea: operators compose with motions. `dd` is not a special
case, it is operator `d` over a linewise current-line motion, which is why most
new commands are a table entry in `src/vim/motions.cpp` rather than a branch in
the state machine.
