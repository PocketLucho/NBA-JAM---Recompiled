#!/usr/bin/env python3
"""lockstep disassembly compare: report first place where instruction kind/length differ"""
import sys, subprocess, re
D = '/home/claude/jam/build/linux/dasm'
def dis(lo, hi, a, b):
    out = subprocess.run([D, 'rom', lo, hi, '%x' % a, '%x' % b], capture_output=True, text=True).stdout.splitlines()
    res = []
    for l in out:
        addr, txt = l.split(':', 1)
        res.append((int(addr, 16), txt.strip()))
    return res
mlo, mhi, rlo, rhi = sys.argv[1:5]
start = int(sys.argv[5], 16); end = int(sys.argv[6], 16)
m = dis(mlo, mhi, start, end); r = dis(rlo, rhi, start, end)
def shape(t):
    return re.sub(r'[0-9A-F]+h', 'N', t)
for i in range(min(len(m), len(r))):
    if m[i][0] != r[i][0] or shape(m[i][1]) != shape(r[i][1]):
        print('first structural diff at index', i)
        for j in range(max(0, i - 6), min(i + 8, len(m), len(r))):
            print('%08X %-40s | %08X %s' % (r[j][0], r[j][1], m[j][0], m[j][1]))
        break
else:
    print('no structural diff')
