#!/usr/bin/env python3
"""check_changelog.py - a pull request must add a line under "Unreleased" in CHANGELOG.md.

    python3 tools/ci/check_changelog.py <base-ref>      # e.g. origin/main
    python3 tools/ci/check_changelog.py --self-test

Passes when the "Unreleased" part of CHANGELOG.md in the working tree has at least one
list item, under one of the known sections, that the base version does not have.
Pull requests that need no entry are marked with the `no-changelog` label; the workflow
skips this check for them.
"""
import re
import subprocess
import sys

SECTIONS = ["Added", "Changed", "Fixed", "Online", "Known issues"]


def unreleased_items(text):
    """{section: [item, ...]} for the Unreleased part. Items are joined over wrapped lines."""
    m = re.search(r"^## +Unreleased\s*$(.*?)(?=^## |\Z)", text, re.M | re.S)
    if not m:
        return None
    items, section, cur = {}, None, None
    for line in m.group(1).splitlines():
        h = re.match(r"^### +(.*?)\s*$", line)
        if h:
            section, cur = h.group(1), None
            items.setdefault(section, [])
        elif re.match(r"^[-*] +\S", line) and section is not None:
            cur = len(items[section])
            items[section].append(line[2:].strip())
        elif line.startswith("  ") and line.strip() and cur is not None:
            items[section][cur] += " " + line.strip()
        elif not line.strip():
            cur = None
    return items


def check(base_text, head_text):
    """(ok, message)"""
    head = unreleased_items(head_text)
    if head is None:
        return False, 'CHANGELOG.md has no "## Unreleased" heading'
    unknown = [s for s in head if s not in SECTIONS]
    if unknown:
        return False, "unknown section(s) under Unreleased: %s (use %s)" % (", ".join(unknown), ", ".join(SECTIONS))
    base = unreleased_items(base_text or "") or {}
    new = [(s, i) for s in head for i in head[s] if i not in base.get(s, [])]
    if not new:
        return False, ("no new line under Unreleased. Add one under %s, or give the pull request "
                       "the no-changelog label if players would not notice the change." % ", ".join(SECTIONS))
    return True, "new entries: " + "; ".join("[%s] %s" % (s, i[:60]) for s, i in new)


def self_test():
    base = "# Changelog\n\n## Unreleased\n\n### Added\n- one\n\n### Fixed\n\n## 1.0.0\n### Added\n- old\n"
    cases = [
        ("no change", base, False),
        ("new item", base.replace("- one\n", "- one\n- two\n"), True),
        ("new item in an empty section", base.replace("### Fixed\n", "### Fixed\n- a crash\n"), True),
        ("item added to an old release only", base + "- sneaked in\n", False),
        ("item reworded", base.replace("- one", "- one, reworded"), True),
        ("item removed only", base.replace("- one\n", ""), False),
        ("wrapped item counts once", base.replace("- one\n", "- one\n- two that\n  wraps\n"), True),
        ("unknown section", base.replace("### Fixed\n", "### Misc\n- x\n"), False),
        ("no Unreleased heading", base.replace("## Unreleased", "## Next"), False),
        ("text that is not a list item", base.replace("### Fixed\n", "### Fixed\nsome words\n"), False),
    ]
    bad = 0
    for name, head, want in cases:
        ok, msg = check(base, head)
        bad += ok != want
        print("%s  %-36s %s" % ("ok  " if ok == want else "FAIL", name, msg[:70]))
    ok, _ = check(None, base)
    bad += not ok
    print("%s  %-36s" % ("ok  " if ok else "FAIL", "first changelog (no base file)"))
    print("%d cases, %d failed" % (len(cases) + 1, bad))
    return 1 if bad else 0


def main():
    if "--self-test" in sys.argv:
        return self_test()
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    base_ref = sys.argv[1]
    r = subprocess.run(["git", "show", base_ref + ":CHANGELOG.md"], capture_output=True)
    base_text = r.stdout.decode("utf-8", "replace") if r.returncode == 0 else None
    try:
        head_text = open("CHANGELOG.md", encoding="utf-8").read()
    except FileNotFoundError:
        print("::error::CHANGELOG.md is missing")
        return 1
    ok, msg = check(base_text, head_text)
    print(msg if ok else "::error file=CHANGELOG.md::" + msg)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
