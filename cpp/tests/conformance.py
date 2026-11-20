#!/usr/bin/env python3
"""Spec <-> SCXML <-> tests traceability check.

Every transition row T-xx in docs/STATECHART-SPEC.md must
  1. appear in statechart/watch.scxml (as <!-- T-xx --> and/or <log label="T-xx"/>),
  2. be checked by an assertion in tests/*.cpp:
       - a row with a <log label="T-xx"/> in the SCXML by FIRED(w, "T-xx"), which
         asserts that the transition actually fired;
       - a row without one (T-15: no transition; T-56: actions of other rows) by
         ROW_CHECK("T-xx", cond).
     Mentions in comments, strings and NOT_FIRED do not count; comments are
     stripped before scanning.
and every <transition> in the SCXML (except the default transition of a
<history>, DESIGN-NOTES D-22, and the host-boundary transitions on host.*
events, D-24) must carry a T-xx that exists in the spec.

Usage: conformance.py [--scxml-only] [--root DIR]
Exit status 0 when everything is traceable, 1 otherwise.
"""
import argparse
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

ROW = re.compile(r"^\|\s*(T-\d+)\s*\|", re.M)
TID = re.compile(r"\bT-\d+\b")
FIRED = re.compile(r"(?<![A-Za-z_])FIRED\(\s*\w+\s*,\s*\"(T-\d+)\"\s*\)")
ROW_CHECK = re.compile(r"\bROW_CHECK\(\s*\"(T-\d+)\"\s*,")
COMMENTS = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
NS = "{http://www.w3.org/2005/07/scxml}"


def key(t):
    return int(t.split("-")[1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scxml-only", action="store_true", help="skip the test-coverage check")
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[2]))
    args = ap.parse_args()
    root = pathlib.Path(args.root)

    spec_text = (root / "docs/STATECHART-SPEC.md").read_text(encoding="utf-8")
    spec = set(ROW.findall(spec_text))

    scxml_path = root / "shared/statechart/watch.scxml"
    scxml_text = scxml_path.read_text(encoding="utf-8")
    in_scxml = set(TID.findall(scxml_text))

    # every <transition> (except history defaults) must contain a <log label="T-xx">
    tree = ET.parse(scxml_path)
    unlabelled = []
    parent = {c: p for p in tree.iter() for c in p}
    for tr in tree.iter(NS + "transition"):
        events = tr.get("event", "").split()
        if parent[tr].tag == NS + "history" or (events and all(e.startswith("host.") for e in events)):
            continue
        labels = [l.get("label", "") for l in tr.iter(NS + "log")]
        ok = [l for l in labels if TID.fullmatch(l) and l in spec]
        if not ok:
            src = parent[tr].get("id")
            unlabelled.append(f"{src} --{tr.get('event', '')}--> {tr.get('target', '(none)')}")

    logged = {l.get("label") for l in tree.iter(NS + "log") if TID.fullmatch(l.get("label", ""))}

    tests, fired, row_checked = set(), set(), set()
    for f in sorted((root / "cpp/tests").glob("*.cpp")):
        text = f.read_text(encoding="utf-8")
        tests |= set(TID.findall(text))
        code = COMMENTS.sub("", text)
        fired |= set(FIRED.findall(code))
        row_checked |= set(ROW_CHECK.findall(code))
    asserted = {t for t in spec if (t in fired if t in logged else t in row_checked)}

    failures = 0

    def report(title, items):
        nonlocal failures
        if items:
            failures += 1
            print(f"FAIL  {title}:")
            for i in items:
                print(f"        {i}")
        else:
            print(f"ok    {title}")

    print(f"spec rows: {len(spec)}  ({', '.join(sorted(spec, key=key))})")
    report("every spec row is implemented in watch.scxml", sorted(spec - in_scxml, key=key))
    report("every T-xx in watch.scxml exists in the spec", sorted(in_scxml - spec, key=key))
    report("every <transition> carries a <log label=\"T-xx\"/> of a spec row", unlabelled)
    if not args.scxml_only:
        report("every spec row with a <log> is asserted with FIRED(w, \"T-xx\")",
               sorted((spec & logged) - asserted, key=key))
        report("every spec row without a <log> is asserted with ROW_CHECK(\"T-xx\", ...)",
               sorted((spec - logged) - asserted, key=key))
        report("ROW_CHECK is only used for rows without a <log> (use FIRED otherwise)",
               sorted(row_checked & logged, key=key))
        report("every T-xx cited in tests exists in the spec", sorted(tests - spec, key=key))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
