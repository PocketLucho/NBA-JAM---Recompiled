#!/usr/bin/env python3
"""compose a raw graphics ROM image from LOAD2 .IRW files (records at bit address 0x2000000+)"""
import struct, sys
def records(path):
    d = open(path, 'rb').read()
    pos = 0
    out = []
    while pos + 0x44 <= len(d):
        addr, length = struct.unpack_from('<II', d, pos + 0x2c)
        body = d[pos + 0x44: pos + 0x44 + length]
        out.append((addr, body))
        pos += 0x44 + length
    return out
def compose(irws, size, base=0x2000000):
    img = bytearray(size)
    for p in irws:
        for addr, body in records(p):
            off = (addr - base) // 8
            img[off:off + len(body)] = body
    return img
if __name__ == '__main__':
    out, size = sys.argv[1], int(sys.argv[2], 0)
    open(out, 'wb').write(compose(sys.argv[3:], size))
