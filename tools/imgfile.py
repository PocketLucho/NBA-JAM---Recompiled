"""Reader for WIMP .IMG files (image records only).

Header: u16 image count, u16 palette count, u32 offset of the image records.
Two record layouts exist in the NBA JAM sources:
  50 bytes (1994+): name[16], flags, anix, aniy, w, h, pal, oset:2, data:2, ?, anix2, aniy2, ?, aniz2, frame, pttbl
  42 bytes (older, e.g. TROGF15.IMG): name[16], anix, aniy, w, h, pal, oset:2, ... (no second anim point)
"""
import struct


def _name(raw):
    return raw[:16].split(b'\0')[0].decode('latin-1').strip().upper()


def _plausible(d, pos):
    raw = d[pos:pos + 16]
    if len(raw) < 16:
        return False
    nm = raw.split(b'\0')[0]
    return 0 < len(nm) <= 15 and all(32 < c < 127 for c in nm)


def records(path):
    """yield dicts: name, anix, aniy, w, h, pt2 (anix2, aniy2, aniz2) or None"""
    d = open(path, 'rb').read()
    n, _, off = struct.unpack_from('<HHI', d, 0)
    size = 50
    if n > 1 and not _plausible(d, off + 50):
        size = next((k for k in (42, 40) if _plausible(d, off + k)), 50)
    for i in range(n):
        r = d[off + i * size: off + i * size + size]
        if len(r) < size:
            break
        if size == 50:
            w = struct.unpack_from('<17h', r, 16)
            yield dict(name=_name(r), anix=w[1], aniy=w[2], w=w[3], h=w[4], pt2=(w[11], w[12], w[14]))
        else:
            w = struct.unpack_from('<%dh' % ((size - 16) // 2), r, 16)
            yield dict(name=_name(r), anix=w[0], aniy=w[1], w=w[2], h=w[3], pt2=None)
