#!/usr/bin/env python3
"""Rewrite a LOAD2 .LOD so every image is packed with the bit depth of its palette.

The 1995 LOAD2 shipped in the sources (NEWLOAD2) packs every image to the fewest bits
that hold its pixels ("PPP> 0").  The tools used for NBA JAM T.E. in 1994 sized the
pixels by the palette instead (bits needed for the palette's colour count): every image
header of the REV 4.0 ROM follows that rule.  This script reproduces it by inserting
"PPP> n" lines before each run of images with the same palette depth.

usage: lod_palbpp.py in.lod out.lod imgdir tables_dir...
  tables_dir: folders with a first-pass LOAD2 output (the first one must be the output
  of this same LOD; it tells which images a whole-file .IMG line really loads) (imgtbl.asm/*.tbl + imgpal.asm),
  used to know each image's palette and the palette sizes.
"""
import math
import os
import re
import sys


# Images whose 1994 depth does not follow the palette rule (checked against the REV 4.0 ROM):
# DIAGPIMG (diagnostics palette holder, 29 colours) was packed at 6 bits per pixel.
OVERRIDE = {'DIAGPIMG': 6}


def img_names(path):
    d = open(path, 'rb').read()
    n = int.from_bytes(d[0:2], 'little')
    off = int.from_bytes(d[4:8], 'little')
    names = []
    for i in range(n):
        raw = d[off + i * 50: off + i * 50 + 16]
        names.append(raw.split(b'\0')[0].decode('latin-1').strip())
    return names


def scan_tables(dirs):
    files = []
    for d in dirs:
        for f in os.listdir(d):
            p = os.path.join(d, f)
            # tables are .asm/.tbl, plus extension-less dump files such as 'junkxxxx'
            if os.path.isfile(p) and (f.lower().endswith(('.asm', '.tbl')) or ('.' not in f and f.lower() != 'l2temp')):
                files.append((f.lower(), open(p, errors='replace').read().replace('\r', '').split('\n')))
    pal_cnt = {}
    for f, lines in files:
        if not f.startswith('imgpal') and not f.startswith('bgndpal'):
            continue
        lab = None
        for l in lines:
            m = re.match(r'^(\w+):', l)
            if m:
                lab = m.group(1)
                continue
            m = re.match(r'\s+\.word\s+(\d+)\s*$', l)
            if lab and m and lab.upper() not in pal_cnt:
                pal_cnt[lab.upper()] = int(m.group(1))
            lab = None
    img_pal = {}
    for f, lines in files:
        if f.startswith('imgpal') or f.startswith('bgndpal'):
            continue
        lab = None
        for l in lines:
            m = re.match(r'^([\w?]+):', l)
            if m:
                lab = m.group(1).upper()
                got = False
                continue
            if lab and not got:
                for tok in re.findall(r'[A-Za-z_]\w*', l.split(';')[0]):
                    if tok.upper() in pal_cnt:
                        img_pal.setdefault(lab, []).append(tok)   # same name may appear twice
                        got = True
                        break
    return img_pal, pal_cnt


def emit(out, names):
    # LOAD2 reads lines into a small buffer: keep image lists short
    line = []
    for n in names:
        if line and len('---> ' + ','.join(line + [n])) > 72:
            out.append('---> ' + ','.join(line))
            line = []
        line.append(n)
    if line:
        out.append('---> ' + ','.join(line))


def main():
    src, dst, imgdir = sys.argv[1:4]
    img_pal, pal_cnt = scan_tables(sys.argv[4:])
    own, _ = scan_tables(sys.argv[4:5])          # first folder = this LOD's own first-pass output
    pal_cnt_u = {k.upper(): v for k, v in pal_cnt.items()}

    seen = {}

    def bpp(name, consume=True):
        base = re.split(r'[:*]', name)[0].strip().upper()
        if base in OVERRIDE:
            return OVERRIDE[base]
        pals = img_pal.get(base)
        if not pals:
            return None
        k = seen.get(base, 0)
        pal = pals[min(k, len(pals) - 1)]
        if consume:
            seen[base] = k + 1
        cnt = pal_cnt_u.get(pal.upper())
        if cnt is None:
            return None
        return max(1, math.ceil(math.log2(max(cnt, 2))))

    lines = open(src, errors='replace').read().replace('\r', '').split('\n')
    out = []
    cur = None
    unknown = []
    i = 0
    while i < len(lines):
        l = lines[i]
        u = l.strip().upper()
        if u.startswith('PPP>'):
            i += 1
            continue                      # we decide the packing ourselves
        if (u.startswith('BBB>') or u.startswith('FRM>')) and cur != 0:
            out.append('PPP> 0')          # backgrounds and movies keep automatic packing
            cur = 0
        if l.startswith('--->') or (u.endswith('.IMG') and not u.startswith(';') and not u[:4] in ('ASM>', 'GLO>')):
            if l.startswith('--->'):
                names = [n.strip() for n in l[4:].split(',') if n.strip()]
            else:
                out.append(l)
                # list mode if any '--->' follows before the next .IMG line
                j = i + 1
                listed = False
                while j < len(lines):
                    t = lines[j].strip().upper()
                    if lines[j].startswith('--->'):
                        listed = True
                        break
                    if t.endswith('.IMG') and not t.startswith(';'):
                        break
                    j += 1
                if listed:
                    i += 1
                    continue
                path = os.path.join(imgdir, l.strip())
                if not os.path.exists(path):
                    alt = [f for f in os.listdir(imgdir) if f.upper() == l.strip().upper()]
                    path = os.path.join(imgdir, alt[0]) if alt else path
                loaded = [n for n in img_names(path) if n.upper() in own]
                bs = set(bpp(n, False) for n in loaded)
                if len(bs) <= 1:
                    for n in loaded:
                        bpp(n)
                    # keep the whole-file load, just set the packing before it
                    b = bs.pop() if bs else 0
                    b = 0 if b is None else b
                    out.insert(len(out) - 1, 'PPP> %d' % b)
                    cur = b
                    i += 1
                    continue
                names = loaded
            run = []
            for n in names:
                b = bpp(n)
                if b is None:
                    unknown.append(n)
                    b = 0                     # no palette in the header: keep automatic packing
                if b != cur and run:
                    emit(out, run)
                    run = []
                if b != cur:
                    out.append('PPP> %d' % b)
                    cur = b
                run.append(n)
            if run:
                emit(out, run)
            i += 1
            continue
        out.append(l)
        i += 1
    open(dst, 'w', newline='\r\n').write('\n'.join(out))
    if unknown:
        print('%s: %d images without palette info: %s' % (src, len(unknown), ' '.join(unknown[:12])), file=sys.stderr)


if __name__ == '__main__':
    main()
