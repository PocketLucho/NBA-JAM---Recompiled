#!/usr/bin/env python3
"""release_notes.py - the changelog at release time (roadmap R2.2).

    python3 tools/ci/release_notes.py cut 1.2.0 [YYYY-MM-DD]
        Before tagging: renames "## Unreleased" in CHANGELOG.md to "## 1.2.0 - date" and puts
        a new, empty "Unreleased" above it. A paragraph written directly under the heading
        (before the first "###") is the summary shown at the top of the release page.

    python3 tools/ci/release_notes.py notes v1.2.0 [--previous v1.1.0] [-o file]
        Prints the release page for that tag: summary, the lists, which download is for which
        system, first-run steps and whether this version plays online with the previous one.
        Uses the changelog section of that version; if the changelog was not cut, it uses
        "Unreleased" and says so on stderr.

    python3 tools/ci/release_notes.py --self-test

Online compatibility is read from the protocol version constant in src/app/netplay.cpp at
this tag and at the previous one: equal means the two versions can play together.
"""
import datetime
import re
import subprocess
import sys

SECTIONS = ["Added", "Changed", "Fixed", "Online", "Known issues"]
EMPTY_UNRELEASED = "## Unreleased\n\n" + "".join("### %s\n\n" % s for s in SECTIONS)


def split_sections(text):
    """[(heading, body)] for every "## " part of the changelog; the text before the first one is dropped."""
    parts = re.split(r"^## +(.*?)\s*$", text, flags=re.M)
    return [(parts[i].strip(), parts[i + 1]) for i in range(1, len(parts), 2)]


def find_version(text, version):
    """Body of the section for `version`, or of Unreleased (second value True) when it was not cut."""
    secs = split_sections(text)
    for head, body in secs:
        if re.match(r"\[?%s\]?(\s|$)" % re.escape(version), head):
            return body, False
    for head, body in secs:
        if head.lower() == "unreleased":
            return body, True
    return None, False


def parse_body(body):
    """(summary, {section: text}) - sections without list items are left out."""
    m = re.search(r"^### ", body, re.M)
    summary = (body[:m.start()] if m else body).strip()
    out = {}
    for name, text in re.findall(r"^### +(.*?)\s*$(.*?)(?=^### |\Z)", body, re.M | re.S):
        if re.search(r"^[-*] +\S", text, re.M):
            out[name] = text.strip("\n")
    return summary, out


def cut(text, version, date):
    m = re.search(r"^## +Unreleased[ 	]*$", text, re.M)
    if not m:
        raise SystemExit('CHANGELOG.md has no "## Unreleased" heading')
    if re.search(r"^## +\[?%s\]?(\s|$)" % re.escape(version), text, re.M):
        raise SystemExit("CHANGELOG.md already has a section for %s" % version)
    return text[:m.start()] + EMPTY_UNRELEASED + "## %s - %s" % (version, date) + text[m.end():]


def online_line(proto_now, proto_prev, previous):
    if previous is None:
        return "This is the first release with online play built this way; all players need this version."
    if proto_now is None or proto_prev is None:
        return "Could not tell whether this version plays online with %s: use the same version on every machine." % previous
    if proto_now == proto_prev:
        return ("**Plays online with %s** (same online protocol, version %s). Every player still needs the same "
                "ROMs and the same Lua mods switched on." % (previous, proto_now))
    return ("**Does not play online with %s** (online protocol %s, was %s). Every player needs this version."
            % (previous, proto_now, proto_prev))


def render(version, summary, sections, online, repo_files=None):
    v = version
    lines = ["# NBA JAM Recomp %s" % v, ""]
    if summary:
        lines += [summary, ""]
    for name in SECTIONS:
        if name in sections:
            lines += ["## %s" % name, sections[name], ""]
    for name in sections:
        if name not in SECTIONS:
            lines += ["## %s" % name, sections[name], ""]
    lines += [
        "## Downloads",
        "| System | File |",
        "|---|---|",
        "| Windows 10/11, 64-bit | `NBAJamRecomp-v%s-windows-x64.zip` |" % v,
        "| Linux, 64-bit (x86-64) | `NBAJamRecomp-v%s-linux-x64.tar.gz` |" % v,
        "",
        "`SHA256SUMS.txt` lists the checksum of each file. Windows may show a SmartScreen warning because "
        "the program is not signed: choose \"More info\", then \"Run anyway\".",
        "",
        "## First run",
        "1. Unpack the archive anywhere.",
        "2. Copy your own ROM sets into the `roms` folder (MAME zips or the source repository zips; see "
        "`README.txt` for the list). No game data is included.",
        "3. Run `NBAJamRecomp`. Pick a game with the arrows and Enter, or a controller.",
        "",
        "Updating from an older version: unpack over the old folder, or into a new one and copy `roms`, "
        "`saves` and `nbajam_recomp.ini` across. The archives keep everything in their own folder.",
        "",
        "## Online play with the previous version",
        online,
        "",
    ]
    return "\n".join(lines)


def git(*args):
    r = subprocess.run(["git"] + list(args), capture_output=True)
    return r.stdout.decode("utf-8", "replace") if r.returncode == 0 else None


def protocol_at(ref):
    src = git("show", "%s:src/app/netplay.cpp" % ref) if ref else None
    if src is None and ref in (None, "HEAD"):
        try:
            src = open("src/app/netplay.cpp", encoding="utf-8").read()
        except OSError:
            src = None
    m = re.search(r"constexpr\s+u16\s+VERSION\s*=\s*(\d+)", src or "")
    return m.group(1) if m else None


def previous_tag(tag):
    tags = (git("tag", "--list", "v*", "--sort=-v:refname") or "").split()
    if tag in tags:
        tags = tags[tags.index(tag) + 1:]
    # test and pre-release tags (v1.2.0-rc1, v0.0.0-test1) are not "the previous version"
    tags = [t for t in tags if "-" not in t and t != tag]
    return tags[0] if tags else None


def self_test():
    log = ("# Changelog\n\nintro\n\n## Unreleased\n\nBig summary line.\n\n### Added\n- a thing\n  wrapped\n\n"
           "### Changed\n\n### Fixed\n- a crash\n\n### Online\n\n### Known issues\n\n## 1.0.0 - 2026-01-01\n\n### Added\n- first\n")
    bad = 0

    def ok(cond, name):
        nonlocal bad
        bad += not cond
        print("%s  %s" % ("ok  " if cond else "FAIL", name))

    body, fell_back = find_version(log, "1.1.0")
    ok(fell_back and "a thing" in body, "uncut changelog: falls back to Unreleased")
    summary, secs = parse_body(body)
    ok(summary == "Big summary line.", "summary is the paragraph under the heading")
    ok(list(secs) == ["Added", "Fixed"], "empty sections are left out")
    new = cut(log, "1.1.0", "2026-12-01")
    ok("## 1.1.0 - 2026-12-01\n\nBig summary line." in new, "cut: Unreleased becomes the version section")
    ok(new.index("## Unreleased") < new.index("## 1.1.0") < new.index("## 1.0.0"), "cut: new empty Unreleased on top")
    ok(parse_body(find_version(new, "Unreleased")[0])[1] == {}, "cut: the new Unreleased is empty")
    body, fell_back = find_version(new, "1.1.0")
    ok(not fell_back and "a thing" in body and "first" not in body, "notes: the version section is found, older ones left out")
    try:
        cut(new, "1.1.0", "x"); ok(False, "cut twice is refused")
    except SystemExit:
        ok(True, "cut twice is refused")
    page = render("1.1.0", summary, secs, online_line("1", "1", "v1.0.0"))
    ok("NBAJamRecomp-v1.1.0-windows-x64.zip" in page and "NBAJamRecomp-v1.1.0-linux-x64.tar.gz" in page, "page names both downloads")
    ok("## First run" in page and "**Plays online with v1.0.0**" in page, "page has first-run steps and the online line")
    ok("Does not play online" in online_line("2", "1", "v1.0.0"), "protocol change is announced")
    ok("first release" in online_line("1", None, None), "no previous version")
    ok(page.index("## Added") < page.index("## Fixed") < page.index("## Downloads"), "sections in the fixed order")
    print("%d failed" % bad)
    return 1 if bad else 0


def main():
    a = sys.argv[1:]
    if a == ["--self-test"]:
        return self_test()
    if len(a) >= 2 and a[0] == "cut":
        version = a[1].lstrip("v")
        date = a[2] if len(a) > 2 else datetime.date.today().isoformat()
        text = open("CHANGELOG.md", encoding="utf-8").read()
        open("CHANGELOG.md", "w", encoding="utf-8", newline="\n").write(cut(text, version, date))
        print("CHANGELOG.md: Unreleased is now %s - %s. Commit, then tag v%s." % (version, date, version))
        return 0
    if len(a) >= 2 and a[0] == "notes":
        tag = a[1]
        version = tag.lstrip("v")
        prev = a[a.index("--previous") + 1] if "--previous" in a else previous_tag(tag)
        out = a[a.index("-o") + 1] if "-o" in a else None
        body, fell_back = find_version(open("CHANGELOG.md", encoding="utf-8").read(), version)
        if body is None:
            raise SystemExit("CHANGELOG.md has neither a %s section nor Unreleased" % version)
        if fell_back:
            print('note: no "%s" section in CHANGELOG.md, using Unreleased (run "cut" before tagging)' % version, file=sys.stderr)
        summary, secs = parse_body(body)
        now = protocol_at(tag) or protocol_at("HEAD")
        page = render(version, summary, secs, online_line(now, protocol_at(prev) if prev else None, prev))
        if out:
            open(out, "w", encoding="utf-8", newline="\n").write(page)
        else:
            sys.stdout.write(page)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
