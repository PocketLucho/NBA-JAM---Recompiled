#!/usr/bin/env bash
# package.sh - make the release packages in releases/
#   tools/package.sh <NBAJamRecomp.exe> <linux NBAJamRecomp> <static emu2> <emu2-src.tar.gz>
set -e
ROOT=$(dirname "$(dirname "$(realpath "$0")")")
EXE=$(realpath "$1"); LIN=$(realpath "$2"); EMU2=$(realpath "$3"); EMU2SRC=$(realpath "$4")
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$ROOT/releases"
for os in windows linux; do
  P=$T/$os/NBAJamRecomp
  mkdir -p "$P/roms" "$P/saves" "$P/tools/gspasm" "$P/tools/emu2"
  cp "$ROOT"/dist/NBAJamRecomp/{LEEME.txt,README.txt} "$P/"
  cp "$ROOT/dist/NBAJamRecomp/roms/LEEME_README.txt" "$P/roms/"
  cp "$ROOT"/tools/{build_data.sh,build_roms.sh,lod_palbpp.py,fix_narrow.py,fix_pwrd.py,imgfile.py,irw2gfx.py,irw2gfx_wolf.py,emu2-ems.patch} "$P/tools/"
  cp "$ROOT/tools/gspasm/gspasm.py" "$P/tools/gspasm/"
  cp "$ROOT"/tools/emu2/{README.txt,LICENSE} "$EMU2SRC" "$P/tools/emu2/"
  cp "$EMU2" "$P/tools/emu2/emu2"
  chmod 755 "$P"/tools/*.sh "$P/tools/emu2/emu2"
done
cp "$EXE" "$T/windows/NBAJamRecomp/"
cp "$LIN" "$T/linux/NBAJamRecomp/"; chmod 755 "$T/linux/NBAJamRecomp/NBAJamRecomp"
rm -f "$ROOT/releases/NBAJamRecomp-windows-x64.zip" "$ROOT/releases/NBAJamRecomp-linux-x64.tar.gz"
( cd "$T/windows" && zip -q -r "$ROOT/releases/NBAJamRecomp-windows-x64.zip" NBAJamRecomp )
( cd "$T/linux" && tar czf "$ROOT/releases/NBAJamRecomp-linux-x64.tar.gz" NBAJamRecomp )
ls -la "$ROOT/releases"
