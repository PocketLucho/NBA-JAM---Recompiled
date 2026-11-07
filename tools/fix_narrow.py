#!/usr/bin/env python3
"""Undo the 1995 LOAD2's minimum image width of 3 pixels.

The LOAD2 in the sources (NEWLOAD2, 1995) stores images narrower than 3 pixels with an
extra blank column.  The 1994 tools used for NBA JAM T.E. did not: in the REV 4.0 ROM the
narrow font glyphs (I, l, !, :, ; ...) are 2 pixels wide.  Every padded glyph moves all the
image data after it, so the ROM addresses no longer match.

This script removes the padding column from the image data of one .IRW and fixes the
widths and all image addresses in the table files LOAD2 wrote next to it.

usage: fix_narrow.py file.irw tables_dir imgdir lod...
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import imgfile  # noqa: E402


def img_dims(imgdir, lods):
    files = {f.upper(): os.path.join(imgdir, f) for f in os.listdir(imgdir)}
    dims = {}
    for lod in lods:
        for l in open(lod, errors='replace').read().replace('\r', '').split('\n'):
            u = l.strip().upper()
            if not u.endswith('.IMG') or u.startswith(';') or u not in files:
                continue
            for r in imgfile.records(files[u]):
                # (true width, height, anix, aniy)
                dims.setdefault(r['name'], set()).add((r['w'], r['h'], r['anix'], r['aniy']))
    return dims


ENTRY = re.compile(r'^([\w?]+):\n\s+\.word\s+(\d+),(\d+),(-?\d+),(-?\d+)[^\n]*\n\s+\.long\s+0?([0-9a-fA-F]+)H\n\s+\.word\s+0?([0-9a-fA-F]+)H',
                   re.M)


def main():
    irw, tdir, imgdir, lods = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    raw = bytearray(open(irw, 'rb').read())
    base, length = struct.unpack_from('<II', raw, 0x2c)
    data = int.from_bytes(raw[0x44:0x44 + length], 'little')
    end = base + length * 8
    dims = img_dims(imgdir, lods)

    tables = [os.path.join(tdir, f) for f in sorted(os.listdir(tdir))
              if os.path.isfile(os.path.join(tdir, f)) and f.lower() != 'l2temp']
    narrow = {}          # address -> (padded w, true w, h, bpp)
    for t in tables:
        txt = open(t, errors='replace').read()
        for m in ENTRY.finditer(txt):
            lab = m.group(1).upper()
            w, h, ax, ay = (int(m.group(k)) for k in range(2, 6))
            addr, ctrl = int(m.group(6), 16), int(m.group(7), 16)
            if w != 3 or not (base <= addr < end):
                continue
            cands = [d for d in dims.get(lab, ()) if d[1:] == (h, ax, ay) and d[0] < w]
            if len(cands) == 1 and not any(d[1:] == (h, ax, ay) and d[0] == w for d in dims.get(lab, ())):
                bpp = (ctrl >> 12) & 7 or 8
                narrow[addr] = (w, cands[0][0], h, bpp)
    if not narrow:
        print('fix_narrow: %s: nothing to do' % os.path.basename(irw))
        return

    # rebuild the bit stream without the padding columns
    out, opos, pos = 0, 0, base
    removed = []                                 # (address, bits removed so far after it)
    total = 0
    mask = lambda n: (1 << n) - 1
    for addr in sorted(narrow):
        w, tw, h, bpp = narrow[addr]
        n = addr - pos
        out |= ((data >> (pos - base)) & mask(n)) << opos
        opos += n
        for row in range(h):
            rbits = (data >> (addr - base + row * w * bpp)) & mask(w * bpp)
            if rbits >> (tw * bpp):
                print('fix_narrow: warning: padding of the image at %x is not blank' % addr, file=sys.stderr)
            out |= (rbits & mask(tw * bpp)) << opos
            opos += tw * bpp
        pos = addr + w * h * bpp
        total += (w - tw) * h * bpp
        removed.append((addr, total))
    n = end - pos
    out |= ((data >> (pos - base)) & mask(n)) << opos
    opos += n

    def remap(a):
        if not (base <= a < end):
            return a
        d = 0
        for addr, tot in removed:
            if addr < a:
                d = tot
        return a - d

    new_len = (opos + 7) // 8
    raw = raw[:0x44] + out.to_bytes(new_len, 'little') + raw[0x44 + length:]
    struct.pack_into('<I', raw, 0x30, new_len)
    open(irw, 'wb').write(raw)

    for t in tables:
        txt = open(t, errors='replace').read()
        # widths of the narrow images
        def fixw(m):
            addr = int(m.group(6), 16)
            if addr in narrow and int(m.group(2)) == narrow[addr][0]:
                s = m.group(0)
                return s.replace('.word   %s,' % m.group(2), '.word   %d,' % narrow[addr][1], 1)
            return m.group(0)
        txt2 = ENTRY.sub(fixw, txt)
        # every image address in this IRW
        txt2 = re.sub(r'(\.long\s+)0?([0-9a-fA-F]{7,8})H',
                      lambda m: m.group(1) + '0%07xH' % remap(int(m.group(2), 16)), txt2)
        if txt2 != txt:
            open(t, 'w').write(txt2)
    print('fix_narrow: %s: %d narrow images, %d bits removed' % (os.path.basename(irw), len(narrow), total))


if __name__ == '__main__':
    main()
