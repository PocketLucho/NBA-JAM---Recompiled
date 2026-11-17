#!/bin/bash
# build_all.sh <linux|win> <sdl_dir> <exe_out>
# What CI does for one platform, runnable on any Ubuntu/WSL box:
#   linux: libcore, liblua, the app and the ROM-free tests (luatest)
#   win:   the same libraries and the app, cross-compiled with mingw-w64
set -euo pipefail
cd "$(dirname "$0")/../.."
target=$1; sdl=$(realpath "$2"); exe_out=$3
j=$(nproc)
case $target in
  linux)
    make -j"$j" BUILD=build/linux build/linux/libcore.a build/linux/liblua.a build/linux/modtest build/linux/luatest build/linux/pathtest build/linux/newstest build/linux/uitest build/linux/arttest
    make BUILD=build/linux SDL2L="$sdl" linux-app
    make BUILD=build/linux SDL2L="$sdl" build/linux/viewtest
    cp build/linux/NBAJamRecomp "$exe_out"
    ;;
  win)
    export PATH="$PWD/build/mingw-bin:$PATH"   # posix-threads variants first (winpthread, as the Makefile links)
    mkdir -p build/mingw-bin
    for t in g++ gcc; do ln -sf "$(command -v x86_64-w64-mingw32-$t-posix)" build/mingw-bin/x86_64-w64-mingw32-$t; done
    make -j"$j" BUILD=build/win CXX=x86_64-w64-mingw32-g++ CC=x86_64-w64-mingw32-gcc build/win/libcore.a
    make WIN_OUT=build/win-out SDL2W="$sdl" win
    cp build/win-out/NBAJamRecomp.exe "$exe_out"
    ;;
  *) echo "usage: $0 <linux|win> <sdl_dir> <exe_out>" >&2; exit 2 ;;
esac
