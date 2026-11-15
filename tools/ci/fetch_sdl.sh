#!/bin/bash
# fetch_sdl.sh <linux|win> <install_dir>
# Downloads the pinned SDL2 release, checks its SHA-256 and prepares it where the
# Makefile expects it:
#   linux: static libSDL2.a built from source  -> <install_dir>/{include,lib}   (make SDL2L=<install_dir>)
#   win:   official MinGW development package  -> <install_dir>/{include,lib}   (make SDL2W=<install_dir>)
# Skipped when <install_dir> already holds a build made by this script version.
set -euo pipefail
SDL_VERSION=2.30.9
SRC_SHA256=24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4      # SDL2-2.30.9.tar.gz
MINGW_SHA256=b188d165dc4a0372042bff3d0c18e9bb90772581e92a7b8b47b521b47714f75c    # SDL2-devel-2.30.9-mingw.tar.gz
BASE=https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION

kind=$1; out=$(realpath -m "$2")
stamp="$out/.fetched-$SDL_VERSION-$kind"
[ -f "$stamp" ] && { echo "SDL2 $SDL_VERSION ($kind) already in $out"; exit 0; }
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fetch() { # file sha
  curl -fsSL --retry 3 -o "$tmp/$1" "$BASE/$1"
  echo "$2  $tmp/$1" | sha256sum -c -
}
rm -rf "$out"; mkdir -p "$out"
case $kind in
  linux)
    fetch SDL2-$SDL_VERSION.tar.gz $SRC_SHA256
    tar -xzf "$tmp/SDL2-$SDL_VERSION.tar.gz" -C "$tmp"
    ( cd "$tmp/SDL2-$SDL_VERSION" && ./configure --prefix="$out" --disable-shared --enable-static >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null )
    ;;
  win)
    fetch SDL2-devel-$SDL_VERSION-mingw.tar.gz $MINGW_SHA256
    tar -xzf "$tmp/SDL2-devel-$SDL_VERSION-mingw.tar.gz" -C "$tmp"
    cp -r "$tmp/SDL2-$SDL_VERSION/x86_64-w64-mingw32/." "$out/"
    ;;
  *) echo "usage: $0 <linux|win> <dir>" >&2; exit 2 ;;
esac
touch "$stamp"
echo "SDL2 $SDL_VERSION ($kind) ready in $out"
