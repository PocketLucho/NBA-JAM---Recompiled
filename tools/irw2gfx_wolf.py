#!/usr/bin/env python3
"""compose a Wolf-unit graphics image (3 banks x 8MB) from dual-banked LOAD2 IRWs"""
import struct, sys
def records(path):
    d = open(path, 'rb').read(); pos = 0; out = []
    while pos + 0x44 <= len(d):
        addr, length = struct.unpack_from('<II', d, pos + 0x2c)
        bank = struct.unpack_from('<H', d, pos + 0x3c)[0]
        out.append((addr, bank, d[pos + 0x44: pos + 0x44 + length])); pos += 0x44 + length
    return out
img = bytearray(0x2000000)
for p in sys.argv[2:]:
    for addr, bank, body in records(p):
        if addr < 0x2000000 or addr >= 0x6000000: continue
        off = bank * 0x800000 + (addr - 0x2000000) // 8
        img[off:off + len(body)] = body
open(sys.argv[1], 'wb').write(img)
