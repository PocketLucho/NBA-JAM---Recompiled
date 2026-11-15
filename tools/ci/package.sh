#!/bin/bash
# package.sh <windows|linux> <executable> <package-name> <out_dir>
# Builds <out_dir>/<package-name>.zip (windows) or .tar.gz (linux) from dist/NBAJamRecomp
# (docs, mods, empty roms/ folder) plus the executable. Never touches game data.
set -euo pipefail
os=$1; exe=$2; name=$3; out=$(realpath -m "$4")
root=$(cd "$(dirname "$0")/../.." && pwd)
stage=$(mktemp -d); trap 'rm -rf "$stage"' EXIT
pkg=$stage/NBAJamRecomp
mkdir -p "$pkg" "$out"
cp "$root"/dist/NBAJamRecomp/LEEME.txt "$root"/dist/NBAJamRecomp/README.txt "$pkg/"
cp -r "$root"/dist/NBAJamRecomp/mods "$pkg/mods"
mkdir -p "$pkg/roms"; cp "$root"/dist/NBAJamRecomp/roms/LEEME_README.txt "$pkg/roms/"
case $os in
  windows) cp "$exe" "$pkg/NBAJamRecomp.exe" ;;
  linux)   cp "$exe" "$pkg/NBAJamRecomp"; strip "$pkg/NBAJamRecomp" ;;
esac
# nothing that looks like game data may end up in a package
if find "$pkg" -type f \( -iname '*.bin' -o -iname '*.rom' -o -iname '*.irw' -o -iname '*.u[0-9]*' -o -iname '*.zip' \) | grep -q .; then
  echo "package contains ROM-like files, refusing" >&2; exit 1
fi
case $os in
  windows) ( cd "$stage" && zip -qr "$out/$name.zip" NBAJamRecomp ); echo "$out/$name.zip" ;;
  linux)   tar -C "$stage" -czf "$out/$name.tar.gz" NBAJamRecomp; echo "$out/$name.tar.gz" ;;
  *) echo "unknown os $os" >&2; exit 2 ;;
esac
