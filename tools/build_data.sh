#!/usr/bin/env bash
# =============================================================================
#  build_data.sh - build the games from their source code into data/
#
#  Usage (from the program folder):   tools/build_data.sh [folder_with_the_zips]
#
#  Put the ZIPs of the three source repositories (GitHub "Code -> Download ZIP")
#  in roms/ (or pass another folder). Unpacked repository folders work too:
#      nba-jam-tournament-edition-main.zip      (historicalsource)
#      nba-jam-tournament-edition-SE-main.zip   (Asure)
#      nba-hangtime-main.zip                    (historicalsource; has Midway's LOAD2.EXE)
#
#  Needs Linux x86-64 with bash and python3 (SteamOS has both). On Windows run it
#  in WSL. It takes a few minutes and about 4 GB of temporary space in the program folder (or $TMPDIR).
#  The result in data/ is byte-identical to the original ROMs (see build_roms.sh);
#  the app uses it when the ROMs themselves are not in roms/.
# =============================================================================
set -e
TOOLS=$(dirname "$(realpath "$0")")
APP=$(dirname "$TOOLS")
SRC=$(realpath "${1:-$APP/roms}")
WORK=$(mktemp -d "${TMPDIR:-$APP}/nbajam_build.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

command -v python3 >/dev/null || { echo "python3 is needed / hace falta python3"; exit 1; }
EMU2=${EMU2:-$TOOLS/emu2/emu2}
[ -x "$EMU2" ] || EMU2=$(command -v emu2 || true)
[ -x "$EMU2" ] || { echo "emu2 not found (tools/emu2/emu2) / no se encuentra emu2"; exit 1; }

echo "Looking for the repositories in / buscando los repositorios en: $SRC"
python3 - "$SRC" "$WORK/repos" <<'EOF'
import os, sys, zipfile, shutil
src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)
# repository name -> a file that must be in it
want = {
    'nba-jam-tournament-edition-SE': 'BB.CMD',
    'nba-jam-tournament-edition': 'ROMKIT/REV47M.0',
    'nba-hangtime': 'NEWLOAD2/LOAD2.EXE',
}
def which(top):
    t = top.lower()
    for name in want:                      # the SE name first: it starts like the T.E. one
        n = name.lower()
        if t == n or t.startswith(n + '-'):
            if name == 'nba-jam-tournament-edition' and t.startswith('nba-jam-tournament-edition-se'):
                continue
            return name
    return None
found = {}
for entry in sorted(os.listdir(src)):
    p = os.path.join(src, entry)
    if os.path.isdir(p):
        name = which(entry)
        if name and name not in found and os.path.exists(os.path.join(p, want[name])):
            os.symlink(os.path.abspath(p), os.path.join(dst, name)); found[name] = entry
    elif entry.lower().endswith('.zip'):
        try:
            z = zipfile.ZipFile(p)
        except zipfile.BadZipFile:
            continue
        names = z.namelist()
        tops = {n.split('/')[0] for n in names if '/' in n}
        if len(tops) != 1:
            continue
        top = tops.pop(); name = which(top)
        if not name or name in found or top + '/' + want[name] not in names:
            continue
        print('  unpacking / descomprimiendo', entry)
        z.extractall(dst)
        os.rename(os.path.join(dst, top), os.path.join(dst, name)); found[name] = entry
missing = [n for n in want if n not in found]
for n in found:
    print('  %-32s <- %s' % (n, found[n]))
if missing:
    print('Missing / faltan:', ', '.join(missing)); sys.exit(1)
EOF

WORK="$WORK/build" bash "$TOOLS/build_roms.sh" "$WORK/repos" "$EMU2" "$APP/data"
echo
echo "data/ is ready / data/ esta listo: $APP/data"
