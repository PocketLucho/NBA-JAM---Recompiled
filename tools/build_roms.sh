#!/bin/bash
# =============================================================================
#  build_roms.sh - rebuild the games from the original source code into data/
#
#  Usage:   tools/build_roms.sh <repos_dir> <emu2> <data_output_dir>
#
#    <repos_dir> holds clones of:
#        nba-jam-tournament-edition      (historicalsource)
#        nba-jam-tournament-edition-SE   (Asure)
#        nba-hangtime                    (historicalsource)
#    <emu2>  emu2 binary (github.com/dmsc/emu2) with tools/emu2-ems.patch applied:
#            LOAD2.EXE (Midway's DOS image loader) needs EMS memory.
#    <data_output_dir>  usually <app folder>/data
#
#  Needs bash and python3. Tested on Linux; on Windows use WSL.
#  The app only uses data/<id>/ when it does not find the game's ROMs in roms/.
#
#  Verified result:
#    - SE  : program ROM byte-identical to l4_* of the SE repository (rev 5.02)
#    - MHT : program ROM byte-identical to ROMS/7M_13M (Maximum Hangtime 1.3)
#    - TE  : program and graphics byte-identical to ROMKIT/REV47M + *.IRW (rev 4.0),
#            thanks to lod_palbpp.py, fix_narrow.py and fix_pwrd.py (see their comments).
# =============================================================================
set -e
REPOS=$(realpath "$1"); EMU2=$(realpath "$2"); OUT=$(realpath -m "$3")
TOOLS=$(dirname "$(realpath "$0")")
GSPASM="python3 $TOOLS/gspasm/gspasm.py"
WORK=${WORK:-/tmp/nbajam_build}
rm -rf "$WORK"; mkdir -p "$WORK" "$OUT"

interleave() { # lo hi out  -> 16-bit interleaved program
python3 - "$@" <<'EOF'
import sys
a=open(sys.argv[1],'rb').read(); b=open(sys.argv[2],'rb').read()
d=bytearray(len(a)*2); d[0::2]=a; d[1::2]=b; open(sys.argv[3],'wb').write(d)
EOF
}
L2() { # dir lod args...  (LOAD2 under emu2, drive C: = dir)
  local dir=$1; shift
  ( cd "$dir" && EMU2_DRIVE_C="$dir" EMU2_DEFAULT_DRIVE=C timeout 900 "$EMU2" LOAD2.EXE "$@" )
}
upcopy() { # copy files with upper-case names: src... dst
  local dst=${@: -1}
  for f in "${@:1:$#-1}"; do cp "$f" "$dst/$(basename "$f" | tr a-z A-Z)"; done
}

# -----------------------------------------------------------------------------
echo "== NBA JAM T.E. Special Edition (rev 5.02)"
SE=$REPOS/nba-jam-tournament-edition-SE
mkdir -p "$WORK/se"
( cd "$SE" && $GSPASM . BB.CMD "$WORK/se/se" --no-preasm --ext .axx --asm IMGTBL,BGNDTBL --irw rom/BBVDA.IRW )
cmp "$WORK/se/se.lo" "$SE/rom/l4_nba_jam_tournament_game_rom_uj12.uj12" && echo "   SE program identical to the original"
mkdir -p "$OUT/nbajamse/sound"
interleave "$WORK/se/se.lo" "$WORK/se/se.hi" "$OUT/nbajamse/program.bin"
cat "$SE"/rom/se1.bin "$SE"/rom/se2.bin "$SE"/rom/se3.bin "$SE"/rom/se4.bin > "$WORK/se/gfx.bin"
# se*.bin (a MAME dump) predates the faces of Jordan, Laimbeer, Petrovic...: the current
# BB3 (img/bb3/BB3.IRW, identical to LOAD2's output for img/bb3/BB3.LOD and consistent with
# the program's PLYRHD4.TBL) is laid over it
python3 - "$WORK/se/gfx.bin" "$SE/img/bb3/BB3.IRW" "$OUT/nbajamse/gfx.bin" <<'PYEOF'
import struct, sys
g = bytearray(open(sys.argv[1], 'rb').read()); d = open(sys.argv[2], 'rb').read(); pos = 0
while pos + 0x44 <= len(d):
    a, l = struct.unpack_from('<II', d, pos + 0x2c); off = (a - 0x2000000) // 8
    g[off:off + l] = d[pos + 0x44:pos + 0x44 + l]; pos += 0x44 + l
open(sys.argv[3], 'wb').write(g)
PYEOF
cp "$SE/rom/ROM5.02/l1_nba_jam_tournament_u3_sound_rom.u3"   "$OUT/nbajamse/sound/u3.bin"
cp "$SE/rom/ROM5.02/l1_nba_jam_tournament_u12_sound_rom.u12" "$OUT/nbajamse/sound/u12.bin"
cp "$SE/rom/ROM5.02/l1_nba_jam_tournament_u13_sound_rom.u13" "$OUT/nbajamse/sound/u13.bin"

# -----------------------------------------------------------------------------
echo "== NBA JAM Tournament Edition (rev 4.0)"
TE=$REPOS/nba-jam-tournament-edition
I=$WORK/teimg; B=$WORK/te
mkdir -p "$I" "$B"
cp "$TE"/IMG/* "$I"/ 2>/dev/null || true
cp "$REPOS"/nba-hangtime/NEWLOAD2/LOAD2.EXE "$I"/
# The 1994 T.E. was packed by an older LOAD2 that gives every image as many bits per pixel
# as its palette needs; the 1995 LOAD2 in the repository uses the minimum. Without this the
# code (which expects e.g. 6-bit name plates) draws noise. Pass 1: LOAD2 with the palette in
# every header to learn each image's palette; pass 2: a LOD with "PPP> n" per image.
INFO=$WORK/teinfo; mkdir -p "$INFO"; cp "$I"/* "$INFO"/ 2>/dev/null || true
for f in BB BB2 BB3 BBMUG BBPAL; do
  python3 - "$INFO/$f.LOD" <<'PYEOF'
import sys
p = sys.argv[1]; s = open(p, errors='replace').read().replace('\r', '')
out = []
for l in s.split('\n'):
    if l.startswith('POF>'): l = 'PON>'
    if l.startswith('IHDR') and 'PAL:L' not in l: l += ',PAL:L'
    out.append(l)
open(p, 'w', newline='\r\n').write('\n'.join(out))
PYEOF
done
for s in "BB:I1" "BB2:I2" "BB3:I3" "BBMUG:I4" "BBPAL:I5"; do
  lod=${s%%:*}; d=${s#*:}; rm -rf "$INFO/$d"; mkdir "$INFO/$d"
  L2 "$INFO" $lod /DI /FI /T=C:\\$d /X > "$INFO/$lod.log" 2>&1 || { echo "LOAD2 $lod (info) failed"; exit 1; }
done
PB="python3 $TOOLS/lod_palbpp.py"
$PB "$TE/IMG/BB.LOD"    "$I/BB.LOD"    "$I" "$INFO/I1" "$INFO/I2" "$INFO/I3" "$INFO/I4" "$INFO/I5"
$PB "$TE/IMG/BB2.LOD"   "$I/BB2.LOD"   "$I" "$INFO/I2" "$INFO/I1" "$INFO/I3" "$INFO/I4" "$INFO/I5"
$PB "$TE/IMG/BB3.LOD"   "$I/BB3.LOD"   "$I" "$INFO/I3" "$INFO/I1" "$INFO/I2" "$INFO/I4" "$INFO/I5"
$PB "$TE/IMG/BBMUG.LOD" "$I/BBMUG.LOD" "$I" "$INFO/I4" "$INFO/I1" "$INFO/I2" "$INFO/I3" "$INFO/I5"
# LOAD2 only takes 8.3 directory names
for s in "BB:OUT" "BB2:OUT_BB2" "BB3:OUT_BB3" "BBMUG:O_MUG" "BBPAL:O_PAL"; do
  lod=${s%%:*}; d=${s#*:}; rm -rf "$I/$d"; mkdir "$I/$d"
  L2 "$I" $lod /DI /FI /T=C:\\$d /X > "$I/$lod.log" 2>&1 || { echo "LOAD2 $lod failed"; exit 1; }
done
# The 1995 LOAD2 pads narrow images (glyphs I, l, !, :) to 3 pixels; the 1994 one did not.
# Remove the padding column and move the image addresses back.
python3 "$TOOLS/fix_narrow.py" "$I/bb.irw"  "$I/OUT"     "$I" "$I/BB.LOD"
python3 "$TOOLS/fix_narrow.py" "$I/bb2.irw" "$I/OUT_BB2" "$I" "$I/BB2.LOD"
cp -r "$TE"/. "$B"/; rm -rf "$B/IMG" "$B/ROMKIT" "$B/.git"
upcopy "$I"/OUT/* "$B"
cp "$I/OUT_BB2/imgpal.asm" "$B/IMGPAL2.ASM"; cp "$I/OUT_BB2/imgtbl.glo" "$B/IMGTBL2.GLO"
for f in ball crowd layup leds pass plyrdseq plyrseq plyrseq2 text; do cp "$I/OUT_BB2/$f.tbl" "$B/$(echo $f | tr a-z A-Z).TBL"; done
cp "$I/OUT_BB2/plyrd2seq.tbl" "$B/PLYRD2SE.TBL"
cp "$I/OUT_BB3/imgpal.asm" "$B/IMGPAL3.ASM"; cp "$I/OUT_BB3/imgtbl4.glo" "$B/IMGTBL4.GLO"
cp "$I/OUT_BB3/fireseq.tbl" "$B/FIRESEQ.TBL"; cp "$I/OUT_BB3/plyrhd4.tbl" "$B/PLYRHD4.TBL"
cp "$I/O_MUG/imgpal.asm" "$B/IMGPALM.ASM"; cp "$I/O_MUG/imgtbl.glo" "$B/IMGTBLM.GLO"; cp "$I/O_MUG/imgtbl3.glo" "$B/IMGTBL3.GLO"
cp "$I/O_MUG/mugshot.tbl" "$B/MUGSHOT.TBL"; cp "$I/O_MUG/plyrhd3.tbl" "$B/PLYRHD3.TBL"
cp "$I/O_PAL/imgpal.asm" "$B/IMGPALP.ASM"; cp "$I/O_PAL/imgtbl.glo" "$B/IMGTBLP.GLO"
# The 1995 LOAD2 writes -1 to PWRD1..3 (second animation point: where the ball sits in the
# hand); take them from the .IMG files. Without this the ball floats and never bounces.
( cd "$B" && python3 "$TOOLS/fix_pwrd.py" "$I" "$I/BB.LOD" "$I/BB2.LOD" "$I/BB3.LOD" "$I/BBMUG.LOD" -- \
    LAYUP.TBL PASS.TBL PLYRDSEQ.TBL PLYRSEQ.TBL PLYRSEQ2.TBL FIRESEQ.TBL PLYRD2SE.TBL PLYRHD4.TBL \
    STAND.TBL BALL.TBL IMGTBL.ASM 2>/dev/null )
( cd "$B" && $GSPASM . BB.CMD "$B/te" )
cmp "$B/te.lo" "$TE/ROMKIT/REV47M.0" && cmp "$B/te.hi" "$TE/ROMKIT/REV47M.1" && echo "   T.E. program identical to REV47M"
mkdir -p "$OUT/nbajamte/sound"
interleave "$B/te.lo" "$B/te.hi" "$OUT/nbajamte/program.bin"
python3 "$TOOLS/irw2gfx.py" "$OUT/nbajamte/gfx.bin" 0x800000 "$I/bb.irw" "$I/bb2.irw" "$I/bb3.irw" "$I/bbmug.irw"
cp "$OUT"/nbajamse/sound/* "$OUT/nbajamte/sound/"
python3 "$TOOLS/irw2gfx.py" "$WORK/te_romkit_gfx.bin" 0x800000 "$TE/ROMKIT/BB.IRW" "$TE/ROMKIT/BB2.IRW" "$TE/ROMKIT/BB3.IRW" "$TE/ROMKIT/BBMUG.IRW"
cmp "$OUT/nbajamte/gfx.bin" "$WORK/te_romkit_gfx.bin" && echo "   T.E. graphics identical to ROMKIT/*.IRW"

# -----------------------------------------------------------------------------
echo "== NBA Maximum Hangtime (1.3)"
HT=$REPOS/nba-hangtime
I=$WORK/htimg; B=$WORK/ht
mkdir -p "$I" "$B"
cp "$HT"/IMG/* "$I"/ 2>/dev/null || true; cp "$HT"/BACKUP/ART110/* "$I"/    # art of version 1.1 (used by 1.3)
cp "$HT"/NEWLOAD2/LOAD2.EXE "$I"/
cd "$I"
step() { # emulates BBL.BAT: lod + renames in the temporary folder
  local lod=$1; shift
  rm -rf BBLTMP; mkdir BBLTMP
  L2 "$I" $lod /E /DI /FI /T=C:\\BBLTMP /X > log_$lod.txt 2>&1 || { echo "LOAD2 $lod failed"; exit 1; }
  while [ $# -gt 0 ]; do
    case $1 in del:*) rm -f BBLTMP/${1#del:};; ren:*) a=${1#ren:}; mv BBLTMP/${a%%=*} BBLTMP/${a#*=};; esac; shift
  done
  mv BBLTMP/* . 2>/dev/null || true
}
step BB    del:imgtbl.asm ren:imgtbl.glo=imgtbl1.glo ren:imgpal.asm=imgpal1.asm
step BB2   del:imgtbl.asm ren:imgtbl.glo=imgtbl2.glo ren:imgpal.asm=imgpal2.asm
step BB3   del:imgtbl.asm del:imgtbl.glo ren:imgpal.asm=imgpal3.asm
step BB4   del:imgtbl.asm del:imgtbl.glo ren:imgpal.asm=imgpal4.asm
step BB5   del:imgtbl.asm del:imgtbl.glo ren:imgpal.asm=imgpal5.asm
step BB6   del:imgtbl.asm del:imgtbl.glo ren:imgpal.asm=imgpal6.asm
step BB7   del:imgtbl.asm ren:bgndpal.asm=bgndpal7.asm ren:bgndtbl.asm=bgndtbl7.asm ren:bgndtbl.glo=bgndtbl7.glo ren:bgndequ.h=bgndequ7.h ren:imgtbl.glo=imgtbl7.glo ren:imgpal.asm=imgpal7.asm
step BB8   del:imgtbl.asm del:imgtbl.glo ren:imgpal.asm=imgpal8.asm
step BBMUG del:imgtbl.asm ren:imgtbl.glo=imgtblm.glo ren:imgpal.asm=imgpalm.asm
rm -rf BBLTMP; mkdir BBLTMP; L2 "$I" BBPAL /E /DI /T=C:\\BBLTMP /X > log_BBPAL.txt 2>&1
rm -f BBLTMP/imgtbl.asm; mv BBLTMP/imgtbl.glo BBLTMP/imgtblp.glo; mv BBLTMP/imgpal.asm BBLTMP/imgpalp.asm; mv BBLTMP/* .
L2 "$I" MISC /E /DI /FI /TC /X > log_MISC.txt 2>&1
rm -rf VDATMP; mkdir VDATMP; L2 "$I" BBVDA /E /DI /FI /T=C:\\VDATMP /X > log_VDA.txt 2>&1
cp "$HT"/SRC/* "$B"/ 2>/dev/null || true
cp "$HT"/BACKUP/CODE113M/* "$B"/                          # source of version 1.3 (Maximum)
for f in *.tbl *.glo *.asm *.h; do [ -f "$f" ] && cp "$f" "$B/$(echo $f | tr a-z A-Z)"; done
cp VDATMP/bbvda.tbl "$B/BBVDA.TBL"
cd "$B" && $GSPASM . BB.CMD "$B/ht" --irw "$I/bbvda.irw"
cmp "$B/ht.lo" "$HT/ROMS/7M_13M.U54" && cmp "$B/ht.hi" "$HT/ROMS/7M_13M.U63" && echo "   Max Hangtime 1.3 program identical to the original"
mkdir -p "$OUT/nbamht" "$OUT/nbahangt"
interleave "$B/ht.lo" "$B/ht.hi" "$OUT/nbamht/program.bin"
python3 "$TOOLS/irw2gfx_wolf.py" "$WORK/ht_gfx.bin" "$I"/bb.irw "$I"/bb2.irw "$I"/bb3.irw "$I"/bb4.irw "$I"/bb5.irw "$I"/bb6.irw "$I"/bb7.irw "$I"/bb8.irw "$I"/bbmug.irw "$I"/misc.irw
head -c 25165824 "$WORK/ht_gfx.bin" > "$OUT/nbamht/gfx.bin"
cp "$OUT/nbamht/gfx.bin" "$OUT/nbahangt/gfx.bin"
interleave "$HT/ROMS/7M_13L.U54" "$HT/ROMS/7M_13L.U63" "$OUT/nbahangt/program.bin"   # NBA Hangtime L1.3 (repo)
echo "Done: $OUT"
