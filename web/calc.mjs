// Everything between xterm.js and the WebAssembly build.
//
// Adapted from FTXUI's own examples/index.mjs, minus the service worker: that
// exists to fake COOP/COEP headers for a threaded build's SharedArrayBuffer, and
// this build is single-threaded on purpose (see web/main.cpp).

import { Terminal } from 'https://cdn.jsdelivr.net/npm/@xterm/xterm@5.5.0/+esm';
import { FitAddon } from 'https://cdn.jsdelivr.net/npm/@xterm/addon-fit@0.10.0/+esm';

// The site's ANSI-16 palette, so calc's colours are the page's colours: dimmed
// comments, light blue functions, yellow variables, magenta constants, cyan
// results, red errors. The 256-colour shades calc uses for the first two come
// from xterm's own extended palette.
const theme = {
  background: '#000000',
  foreground: '#ffffff',
  cursor: '#00ff00',
  cursorAccent: '#000000',
  selectionBackground: '#5555ff',
  black: '#000000',   red: '#aa0000',     green: '#00aa00',   yellow: '#aa5500',
  blue: '#0000aa',    magenta: '#aa00aa', cyan: '#00aaaa',    white: '#aaaaaa',
  brightBlack: '#555555',   brightRed: '#ff0000',
  brightGreen: '#00ff00',   brightYellow: '#ffff00',
  brightBlue: '#5555ff',    brightMagenta: '#ff00ff',
  brightCyan: '#00ffff',    brightWhite: '#ffffff',
};

const element = document.getElementById('terminal');
const term = new Terminal({
  theme,
  fontFamily: "'Cascadia Mono', 'DejaVu Sans Mono', Menlo, Consolas, monospace",
  fontSize: 14,
  // Nothing scrolls back: calc draws a full screen every frame and owns the
  // alternate screen buffer, so a scrollback would only collect torn frames.
  scrollback: 0,
  cursorBlink: false,
  convertEol: false,
});
const fit = new FitAddon();
term.loadAddon(fit);
term.open(element);

// ---------------------------------------------------------------- stdin/stdout
// Emscripten pulls one byte at a time and treats null as "nothing right now",
// which FTXUI's Emscripten branch already expects: it reads non-blocking and
// feeds a timeout to its escape-sequence parser when a read comes back empty.
const stdin_queue = [];
const stdout_bytes = [];
const stderr_bytes = [];

const stdin = () => (stdin_queue.length > 0 ? stdin_queue.shift() : null);

// calc marks the end of a frame with a 0 byte, because Emscripten has no flush.
// Writing per frame rather than per byte is what keeps the render from tearing.
const stdout = code => {
  if (code === 0) {
    term.write(new Uint8Array(stdout_bytes));
    stdout_bytes.length = 0;
  } else {
    stdout_bytes.push(code);
  }
};

// Its own buffer, and a line at a time: stderr is not part of the frame, and
// mixing it into the frame's bytes would corrupt the screen it prints beside.
const stderr = code => {
  if (code === 0 || code === 10) {
    if (stderr_bytes.length > 0) console.error('calc:', String.fromCharCode(...stderr_bytes));
    stderr_bytes.length = 0;
  } else {
    stderr_bytes.push(code);
  }
};

const onData = data => {
  for (let i = 0; i < data.length; ++i) stdin_queue.push(data.charCodeAt(i) & 0xff);
};
term.onData(onData);
term.onBinary(onData);

// ---------------------------------------------------------------- key conflicts
// calc wants Ctrl-R for redo and Ctrl-D/U/E/Y for scrolling; the browser wants
// them for reload, bookmark and view-source. Claim them, but only the five, and
// only while the terminal has focus. Ctrl-W/T/N are reserved by the browser and
// cannot be claimed — calc does not use them.
const claimed = new Set(['r', 'd', 'u', 'e', 'y']);
term.attachCustomKeyEventHandler(event => {
  if (event.type === 'keydown' && event.ctrlKey && !event.metaKey && !event.altKey &&
      claimed.has(event.key.toLowerCase())) {
    event.preventDefault();
  }
  return true;
});

// A visitor whose keystrokes go to the page instead of the terminal thinks it is
// broken, so the frame and the hint both say where the keyboard is going.
const frame = document.getElementById('window');
const hint = document.getElementById('hint');
term.textarea?.addEventListener('focus', () => {
  frame.classList.add('focused');
  hint.classList.add('armed');
  hint.textContent = '▶ TYPE i TO INSERT, ESC TO STOP ◀';
});
term.textarea?.addEventListener('blur', () => {
  frame.classList.remove('focused');
  hint.classList.remove('armed');
  hint.textContent = '► CLICK THE TERMINAL, THEN TYPE ◄';
});

// ---------------------------------------------------------------- the module
let observer = null;

window.Module = {
  preRun: () => { FS.init(stdin, stdout, stderr); },
  onRuntimeInitialized: () => {
    fit.fit();

    // FTXUI reports a fixed fallback size under Emscripten and only learns the
    // real one from this exported function, which raises SIGWINCH for it.
    const resize = () => {
      const size = fit.proposeDimensions();
      if (!size || !size.cols || !size.rows) return;
      term.resize(size.cols, size.rows);
      window.Module._ftxui_on_resize(size.cols, size.rows);
      fit.fit();
    };
    // Held in a variable on purpose: an observer nobody references can be
    // collected while it still has observations, and then the terminal silently
    // stops following the window.
    observer = new ResizeObserver(resize);
    observer.observe(element);
    resize();
    term.focus();
  },
  onAbort: what => {
    console.error('calc aborted:', what);
    document.getElementById('error').style.display = 'block';
  },
};

// Loaded last and by hand: the Emscripten glue reads window.Module the moment it
// runs, and a module script would still be waiting for its imports.
const script = document.createElement('script');
script.src = 'calc.js';
script.onerror = () => { document.getElementById('error').style.display = 'block'; };
document.body.appendChild(script);
