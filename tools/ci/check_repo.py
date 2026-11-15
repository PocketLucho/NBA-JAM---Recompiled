#!/usr/bin/env python3
"""check_repo.py - fail if the repository tracks ROM-like, archive or built binary files.

Roadmap rule 1: no game data in the repository. Builds are published as GitHub Releases,
not committed. Run from anywhere inside the checkout:

    python3 tools/ci/check_repo.py            # checks every tracked file
    python3 tools/ci/check_repo.py --self-test

A file is refused when any of these holds:
  * its extension is an archive, ROM dump, disk image or executable type
  * its first bytes are those of a zip / 7z / rar / gzip / xz / bzip2 archive or a
    Windows / Linux / macOS executable
  * it is binary and its size is a power of two of 32 KB or more (what a ROM chip dump is)
  * it is larger than 2 MB
Paths listed in tools/ci/allowed_files.txt (one per line, # comments) are skipped; adding
one needs a reason in the pull request.
"""
import os
import re
import subprocess
import sys

BAD_EXT = re.compile(
    r"\.(zip|7z|rar|gz|tgz|xz|bz2|tar|bin|rom|irw|img|iso|chd|exe|dll|so|dylib|a|o|obj|lib|"
    r"appimage|apk|dmg|msi|u\d+|uj\d+|ug\d+|nv|sta)$", re.I)
MAGIC = [
    (b"PK\x03\x04", "zip archive"), (b"7z\xbc\xaf\x27\x1c", "7z archive"), (b"Rar!", "rar archive"),
    (b"\x1f\x8b", "gzip data"), (b"\xfd7zXZ", "xz data"), (b"BZh", "bzip2 data"),
    (b"MZ", "Windows executable"), (b"\x7fELF", "ELF executable"),
    (b"\xcf\xfa\xed\xfe", "Mach-O executable"), (b"\xca\xfe\xba\xbe", "Mach-O universal binary"),
]
MAX_SIZE = 2 * 1024 * 1024


def reason(path, size, head):
    """Why this file may not be in the repository, or None."""
    m = BAD_EXT.search(path)
    if m:
        return "extension %s" % m.group(0)
    for magic, what in MAGIC:
        if head.startswith(magic):
            return what
    binary = b"\0" in head
    if binary and size >= 32 * 1024 and size & (size - 1) == 0:
        return "binary of exactly %d KB (ROM-sized)" % (size // 1024)
    if size > MAX_SIZE:
        return "larger than 2 MB (%d KB)" % (size // 1024)
    return None


def self_test():
    cases = [
        ("roms/nbajam.zip", 100, b"PK\x03\x04", True),
        ("x/l3_nba_jam_game_rom_uj12.uj12", 524288, b"\0" * 64, True),
        ("docs/picture.png", 524288, b"\x89PNG\0\0", True),          # ROM-sized binary under another name
        ("docs/notes.txt", 300, b"PK\x03\x04rest", True),            # zip under another name
        ("releases/NBAJamRecomp-linux-x64.tar.gz", 1000, b"\x1f\x8b", True),
        ("tool", 4000, b"\x7fELF\x02", True),
        ("src/app/main.cpp", 80000, b"// main.cpp", False),
        ("src/big_table.cpp", 524288, b"static const int t[] = {", False),   # text, power of two: fine
        ("docs/shot.png", 40000, b"\x89PNG\r\n\x1a\n\0\0", False),
        ("ROADMAP.md", 70000, b"# NBA JAM", False),
        ("huge.txt", 3 * 1024 * 1024, b"aaaa", True),
    ]
    bad = 0
    for path, size, head, want in cases:
        got = reason(path, size, head)
        ok = (got is not None) == want
        bad += not ok
        print("%s  %-45s %s" % ("ok  " if ok else "FAIL", path, got or "allowed"))
    print("%d cases, %d failed" % (len(cases), bad))
    return 1 if bad else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
    os.chdir(root)
    allowed = set()
    if os.path.exists("tools/ci/allowed_files.txt"):
        for line in open("tools/ci/allowed_files.txt", encoding="utf-8"):
            line = line.split("#")[0].strip()
            if line:
                allowed.add(line)
    files = subprocess.check_output(["git", "ls-files", "-z"]).decode("utf-8", "replace").split("\0")
    problems = []
    for f in files:
        if not f or f in allowed or not os.path.isfile(f):
            continue
        with open(f, "rb") as fh:
            head = fh.read(4096)
        why = reason(f, os.path.getsize(f), head)
        if why:
            problems.append((f, why))
    for f, why in problems:
        print("::error file=%s::not allowed in the repository: %s" % (f, why))
    print("%d tracked files checked, %d not allowed" % (len([f for f in files if f]), len(problems)))
    if problems:
        print("ROMs, archives and built binaries do not belong in the repository (ROADMAP.md, rule 1).")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
