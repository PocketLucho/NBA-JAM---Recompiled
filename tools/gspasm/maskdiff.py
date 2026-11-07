#!/usr/bin/env python3
"""diff ignoring relocated fields (as reported by gspasm .rmask); prints first real diffs with source"""
import sys, bisect
pre, rlo, rhi = sys.argv[1], sys.argv[2], sys.argv[3]
n = int(sys.argv[4]) if len(sys.argv) > 4 else 5
def load(lo, hi):
    a = open(lo, 'rb').read(); b = open(hi, 'rb').read(); d = bytearray(len(a) * 2); d[0::2] = a; d[1::2] = b; return d
m = load(pre + '.lo', pre + '.hi'); r = load(rlo, rhi); mask = open(pre + '.rmask', 'rb').read()
lst = []
for l in open(pre + '.lst'):
    a, w = l.split(None, 1); lst.append((int(a, 16), w.strip()))
lst.sort(); keys = [x[0] for x in lst]
cnt = 0; tot = 0; last = -10**9
for i in range(len(r)):
    if mask[i] or m[i] == r[i]: continue
    tot += 1
    if i - last < 32: continue
    last = i
    if cnt < n:
        addr = 0xff800000 + i * 8
        j = bisect.bisect_right(keys, addr) - 1
        print('%08X mine=%02X ref=%02X  %s' % (addr, m[i], r[i], lst[j][1] if j >= 0 else '?'))
        cnt += 1
print('unmasked differing bytes:', tot, ' masked bytes:', sum(mask))
