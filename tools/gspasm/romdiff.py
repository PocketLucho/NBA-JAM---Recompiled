#!/usr/bin/env python3
"""compare a built ROM pair with a reference pair; report first diffs with disassembly"""
import sys, subprocess
def load(lo, hi):
    a = open(lo, 'rb').read(); b = open(hi, 'rb').read()
    d = bytearray(len(a) * 2); d[0::2] = a; d[1::2] = b
    return d
mine = load(sys.argv[1], sys.argv[2]); ref = load(sys.argv[3], sys.argv[4])
n = int(sys.argv[5]) if len(sys.argv) > 5 else 5
diff = [i for i in range(0, len(ref), 2) if mine[i:i+2] != ref[i:i+2]]
print('differing words: %d / %d' % (len(diff), len(ref) // 2))
D = '/home/claude/jam/build/linux/dasm'
shown = 0; last = -1000
for d in diff:
    if d - last < 64: continue
    last = d
    addr = 0xff800000 + d * 8
    a0 = max(0xff800000, addr - 0x60)
    r = subprocess.run([D, 'rom', sys.argv[3], sys.argv[4], '%x' % a0, '%x' % (addr + 0x80)], capture_output=True, text=True).stdout.splitlines()
    m = subprocess.run([D, 'rom', sys.argv[1], sys.argv[2], '%x' % a0, '%x' % (addr + 0x80)], capture_output=True, text=True).stdout.splitlines()
    print('---- diff at %08X' % addr)
    for i in range(max(len(r), len(m))):
        print('%-48s | %s' % (r[i] if i < len(r) else '', m[i] if i < len(m) else ''))
    shown += 1
    if shown >= n: break
