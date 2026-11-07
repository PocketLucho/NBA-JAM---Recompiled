#!/usr/bin/env python3
"""Fill the PWRD1..PWRD3 fields of LOAD2 image tables from the .IMG files.

NBA JAM T.E. (1994) declares image headers with PWRD1:W,PWRD2:W,PWRD3:W - the image's
second animation point (ball hold position: anix2, aniy2, aniz2).  The 1995 LOAD2 from the
sources does not know these fields for the T.E.-era .IMG files and writes -1,-1,-1, which
makes the ball float in the wrong place.  This puts the real values back.

usage: fix_pwrd.py imgdir lod... -- table_file...
"""
import re
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import imgfile  # noqa: E402


def img_records(path):
    for r in imgfile.records(path):
        if r['pt2'] is not None:
            yield r['name'], (r['w'], r['h'], r['anix'], r['aniy']), r['pt2']


def main():
    a = sys.argv[1:]
    sep = a.index('--')
    imgdir, lods, tables = a[0], a[1:sep], a[sep + 1:]
    files = {f.upper(): os.path.join(imgdir, f) for f in os.listdir(imgdir)}
    # image name -> list of point triples, in LOD order
    pts = {}
    seen_img = set()
    for lod in lods:
        for l in open(lod, errors='replace').read().replace('\r', '').split('\n'):
            u = l.strip().upper()
            if u.endswith('.IMG') and not u.startswith(';') and u in files and u not in seen_img:
                seen_img.add(u)
                for name, dims, p in img_records(files[u]):
                    pts.setdefault(name, []).append((dims, p))
    used = {}
    fixed = 0
    for t in tables:
        lines = open(t, errors='replace').read().split('\n')
        lab = None
        dims = None
        for i, l in enumerate(lines):
            m = re.match(r'^([\w?]+):', l)
            if m:
                lab = m.group(1).upper()
                dims = None
                continue
            m = re.match(r'^\s+\.word\s+(-?\d+),(-?\d+),(-?\d+),(-?\d+)\s*$', l)
            if m and lab and dims is None:
                dims = tuple(int(x) for x in m.groups())
            # the PWRD words open the .word line that follows the palette (".long PAL")
            m = re.match(r'^(\s+\.word\s+)(-1,-1,-1)(,.*)$', l)
            if m and lab and i > 0 and re.match(r'\s+\.long\s+[A-Za-z_]', lines[i - 1]):
                lst = pts.get(lab)
                if lst:
                    # same name in several .IMG files: pick the one with the same size and
                    # animation point; otherwise take them in LOD order
                    same = [q for d, q in lst if d == dims]
                    if len(set(same)) == 1:
                        p = same[0]
                    else:
                        k = used.get(lab, 0)
                        p = lst[min(k, len(lst) - 1)][1]
                        used[lab] = k + 1
                    lines[i] = m.group(1) + '%d,%d,%d' % p + m.group(3)
                    fixed += 1
                lab = None
        open(t, 'w').write('\n'.join(lines))
    print('fix_pwrd: %d headers' % fixed)


if __name__ == '__main__':
    main()
