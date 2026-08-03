
set -eu

REPO="Thinato/calc"
BIN="calc"
DEST_DIR="${CALC_INSTALL_DIR:-$HOME/.local/bin}"
TAG="${CALC_VERSION:-latest}"

SOURCE_HINT="git clone https://github.com/$REPO && cd calc && cmake --preset default && cmake --build build"

die() {
    printf 'install.sh: %s\n' "$1" >&2
    exit 1
}

say() {
    printf '%s\n' "$1"
}

require() {
    command -v "$1" >/dev/null 2>&1 || die "missing tool on PATH: $1"
}

require uname
require tar
require mktemp

if command -v curl >/dev/null 2>&1; then
    FETCH=curl
elif command -v wget >/dev/null 2>&1; then
    FETCH=wget
else
    die "need curl or wget on PATH"
fi

fetch() {
    if [ "$FETCH" = curl ]; then
        curl --proto '=https' --tlsv1.2 -fsSL "$1" -o "$2"
    else
        wget --https-only -q -O "$2" "$1"
    fi
}

detect_triple() {
    os="$(uname -s)"
    arch="$(uname -m)"
    case "$os" in
        Linux)
            case "$arch" in
                x86_64|amd64)   echo "x86_64-unknown-linux-gnu" ;;
                aarch64|arm64)  echo "aarch64-unknown-linux-gnu" ;;
                *) die "no published build for Linux/$arch — build from source: $SOURCE_HINT" ;;
            esac
            ;;
        Darwin)
            case "$arch" in
                arm64|aarch64)  echo "aarch64-apple-darwin" ;;
                x86_64)         echo "x86_64-apple-darwin" ;;
                *) die "no published build for macOS/$arch" ;;
            esac
            ;;
        *) die "unsupported OS: $os (only Linux and macOS)" ;;
    esac
}

resolve_tag() {
    if [ "$TAG" != latest ]; then
        echo "$TAG"
        return
    fi
    api="https://api.github.com/repos/$REPO/releases/latest"
    tmp="$(mktemp)"
    fetch "$api" "$tmp" ||
        die "could not read $api — is there a published release yet?"
    out="$(sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$tmp" | head -n1)"
    rm -f "$tmp"
    [ -n "$out" ] || die "could not read tag_name from $api"
    echo "$out"
}

main() {
    triple="$(detect_triple)"
    tag="$(resolve_tag)"
    pkg="${BIN}-${tag}-${triple}"
    url="https://github.com/${REPO}/releases/download/${tag}/${pkg}.tar.gz"

    say "calc ${tag} (${triple})"
    say "fetching ${url}"

    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT INT TERM HUP

    fetch "$url" "$work/pkg.tar.gz" ||
        die "no download at $url — that release may not include $triple"
    tar -C "$work" -xzf "$work/pkg.tar.gz"

    bin_src="$work/${pkg}/${BIN}"
    [ -f "$bin_src" ] || die "binary not found inside archive at $bin_src"

    mkdir -p "$DEST_DIR"
    mv "$bin_src" "$DEST_DIR/$BIN"
    chmod +x "$DEST_DIR/$BIN"

    say ""
    if version="$("$DEST_DIR/$BIN" --version 2>/dev/null)"; then
        say "installed: $DEST_DIR/$BIN ($version)"
    else
        say "installed: $DEST_DIR/$BIN"
        say ""
        say "warning: that binary did not run here. On an older Linux its glibc is"
        say "probably too new; build from source instead:"
        say "  $SOURCE_HINT"
    fi

    case ":${PATH:-}:" in
        *":$DEST_DIR:"*) ;;
        *)
            say ""
            say "note: $DEST_DIR is not on your PATH yet."
            say "add this to your shell rc:"
            say "  export PATH=\"$DEST_DIR:\$PATH\""
            ;;
    esac
}

main "$@"
