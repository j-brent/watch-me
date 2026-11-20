#!/usr/bin/env python3
"""P8 verification walk and cross-host conformance suite: drives the *running app*
(without its window, --no-gui) through its normal input path (ButtonRouter ->
WatchMachine -> TransitionTracer) once per spec row, exports the app's own event
log, and checks it. Writes docs/VERIFICATION.md (from the C++ host only).

Any host that implements docs/HOST-CONTRACT.md (cpp/, python/, rust/, qmljs/,
qmlts/) is checked the same way: give its command line with --cmd.

Usage: tools/verify.py [--bin APP | --cmd "COMMAND"] [--check | --no-doc] [build-dir]
  --bin APP      the watch-me executable (default: <build-dir>/watch-me); ctest passes
                 the real target file, which differs on Windows and macOS.
  --cmd COMMAND  the start command of another host, split like a shell would
                 (e.g. --cmd "python3 python/main.py"); implies --no-doc and
                 cannot be combined with --check.
  --check        do not write docs/VERIFICATION.md; fail if it differs from what this
                 run generates (CI: the committed document must be current).
  --no-doc       do not write or check docs/VERIFICATION.md; only the walk counts.
Exit status 0 only if every row passes, every spec row is walked, and (--check) the
document is current. Runs as the ctest `verify`, so CI runs it on every platform.

Each run starts with the battery inserted at 1987-06-01 10:08:30.
Script steps: 'a' click, '+a'/'-a' hold/release, 'wait:ms',
'set:harelAnomalies:value' (the Fig. 32 flag, sent as host.anomalies; no other
datamodel value can be set, DESIGN-NOTES D-24), 'advance:ms' (the statechart's timeout clock), 'clock:ms' (the
watch's time T), 'alarm:n:HH:MM', 'jump:n:secs', 'settime:H:M:S', or an event name.
"""
import argparse
import concurrent.futures as cf
import difflib
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CMD = [str(ROOT / "build" / "watch-me")]   # the host's start command, set in main()

UPD = "+c;advance:2000;-c"                          # hold c for 2 s: into update.sec
SW = "a;a;a;a"                                       # time -> stopwatch
EN1 = "a;d;a;a;a;a"                                  # enable alarm 1, back to time
EN2 = "a;a;d;a;a;a"                                  # enable alarm 2, back to time

# (row, steps, expected log lines (substrings), required active states, forbidden substrings)
ROWS = [
    ("T-01", "", ["T-01 battery.inserted: dead → alive"], ["time", "bt_00", "power_ok"], []),
    ("T-02", "battery.weakens;battery.dies", ["T-02 battery.dies: power_blink → dead"], ["dead"], []),
    ("T-03", "battery.removed", ["T-03 battery.removed: alive → dead"], ["dead"], []),
    ("T-04", "battery.weakens", ["T-04 battery.weakens: power_ok → power_blink"], ["power_blink"], []),
    ("T-10", "a", ["T-10 a.down: time → alarm1"], ["alarm1_off"], []),
    ("T-11", "a;a", ["T-11 a.down: alarm1 → alarm2"], ["alarm2_off"], []),
    ("T-12", "a;a;a", ["T-12 a.down: alarm2 → chime"], ["chime_off"], []),
    ("T-13", "a;a;a;a", ["T-13 a.down: chime → stopwatch"], ["sw_zero"], []),
    ("T-14", "a;a;a;a;a", ["T-14 a.down: stopwatch → regular"], ["time"], []),
    ("T-15", "d;a;a", ["T-20 d.down: time → date"], ["date"], ["a.down: date →"]),
    ("T-20", "d", ["T-20 d.down: time → date", "T-75 d.down: bt_00 → bt_01"], ["date"], []),
    ("T-21", "d;d", ["T-21 d.down: date → time"], ["time"], []),
    ("T-22", "d;advance:119999;advance:1", ["T-22 timeout.date: date → time"], ["time"], []),
    ("T-23", "+c", ["T-23 c.down: time → wait"], ["wait"], []),
    ("T-24", "+c;-c", ["T-24 c.up: wait → time"], ["time"], []),
    ("T-25", UPD, ["T-25 timeout.wait: wait → update_sec"], ["update_sec"], []),
    ("T-26", UPD + ";c;c;c;c;c;c;c;c;c",
     ["T-26 c.down: update_sec → update_1min", "T-26 c.down: update_10min → update_hr",
      "T-26 c.down: update_year → update_mode", "T-26 c.down: update_mode → time"], ["time"], []),
    ("T-27", UPD + ";c;c;c;c;d", ["◀ model.increment field=update_mon", "T-27 d.down: update → (no state change)"],
     ["update_mon"], []),
    ("T-28", UPD + ";c;c;c;b", ["T-28 b.down: update → time", "T-72 b.down: light_off → light_on"], ["time"], []),
    ("T-29", UPD + ";advance:120000", ["T-29 timeout.update: update → time"], ["time"], []),
    ("T-30", "a;c", ["T-30 c.down: alarm1 → update1_hr"], ["update1_hr"], []),
    ("T-31", "a;c;c;c", ["T-31 c.down: update1_hr → update1_10min", "T-31 c.down: update1_10min → update1_1min"],
     ["update1_1min"], []),
    ("T-32", "a;d;c;c;c;c", ["T-32 c.down: update1_1min → alarm1_H"], ["alarm1_on"], []),
    ("T-33", "a;c;d", ["◀ model.increment field=update1_hr", "T-33 d.down: update1 → (no state change)"],
     ["update1_hr", "alarm1_disabled"], []),
    ("T-34", "a;d;c;c;b", ["T-34 b.down: update1 → alarm1_H"], ["alarm1_on"], []),
    ("T-35", "a;a;c", ["T-35 c.down: alarm2 → update2_hr"], ["update2_hr"], []),
    ("T-36", "a;a;c;c;c", ["T-36 c.down: update2_hr → update2_10min", "T-36 c.down: update2_10min → update2_1min"],
     ["update2_1min"], []),
    ("T-37", "a;a;d;c;c;c;c", ["T-37 c.down: update2_1min → alarm2_H"], ["alarm2_on"], []),
    ("T-38", "a;a;c;c;d", ["◀ model.increment field=update2_10min", "T-38 d.down: update2 → (no state change)"],
     ["update2_10min"], []),
    ("T-39", "a;a;c;b", ["T-39 b.down: update2 → alarm2_H"], ["alarm2_off"], []),
    ("T-40", "a;c;advance:120000", ["T-40 timeout.out: out → regular"], ["time"], []),
    ("T-41", "a;d;d", ["T-41 d.down: alarm1_disabled → alarm1_enabled", "T-41 d.down: alarm1_enabled → alarm1_disabled"],
     ["alarm1_disabled"], []),
    ("T-42", "a;a;d", ["T-42 d.down: alarm2_disabled → alarm2_enabled"], ["alarm2_enabled"], []),
    ("T-43", "a;a;a;d;d;d", ["T-43 d.down: chime_disabled → chime_enabled", "T-43 d.down: chime_enabled → chime_disabled"],
     ["chime_enabled", "chime_quiet"], []),
    ("T-44", "a;d;d", ["T-44 d.down: alarm1_off → alarm1_on", "T-44 d.down: alarm1_on → alarm1_off"], ["alarm1_off"], []),
    ("T-45", "a;a;d", ["T-45 d.down: alarm2_off → alarm2_on"], ["alarm2_on"], []),
    ("T-46", "a;a;a;d", ["T-46 d.down: chime_off → chime_on"], ["chime_on"], []),
    ("T-47", "d;advance:100000;a;advance:119999", ["T-47 a.down: alive → (no state change)"], ["date"],
     ["T-22"]),
    ("T-50", SW + ";b", ["T-50 b.down: sw_zero → sw_reg, sw_on"], ["sw_reg", "sw_on"], []),
    ("T-51", SW + ";b;b", ["T-51 b.down: sw_on → sw_off"], ["sw_off"], []),
    ("T-52", SW + ";b;b;b", ["T-52 b.down: sw_off → sw_on"], ["sw_on"], []),
    ("T-53", SW + ";b;d", ["◀ sw.lap", "T-53 d.down: sw_reg → sw_lap"], ["sw_lap"], []),
    ("T-54", SW + ";b;d;d", ["T-54 d.down: sw_lap → sw_reg"], ["sw_reg"], []),
    ("T-55", SW + ";b;b;d", ["◀ sw.reset", "T-55 d.down: sw_reg → sw_zero"], ["sw_zero"], []),
    ("T-56", SW + ";b;b;b", ["◀ sw.start", "◀ sw.stop", "◀ beep.short"], ["sw_on"], []),
    ("T-60", "alarm:1:10:10;" + EN1 + ";jump:1:2;clock:3000", ["T-60 T.hits.T1: displays → alarm1_beeps"],
     ["alarm1_beeps"], []),
    ("T-61", "alarm:1:09:00;alarm:2:10:10;" + EN1 + ";" + EN2 + ";jump:2:2;clock:3000",
     ["T-61 T.hits.T2: displays → alarm2_beeps"], ["alarm2_beeps"], []),
    ("T-62", "alarm:1:10:10;alarm:2:10:10;" + EN1 + ";" + EN2 + ";jump:1:2;clock:3000",
     ["T-62 T.hits.T1: displays → both_beep"], ["both_beep"], ["T-60", "T-61"]),
    ("T-63", "alarm:1:10:10;" + EN1 + ";a;a;c;c;jump:1:2;clock:3000;c",
     ["T-63 c.down: alarms_beep → displays_H"], ["update2_10min"], []),
    ("T-64", "alarm:1:10:10;" + EN1 + ";" + SW + ";b;d;jump:1:2;clock:3000;advance:30000",
     ["T-64 timeout.alarms_beep: alarms_beep → displays_H"], ["sw_lap", "sw_on"], []),
    ("T-70", "a;a;a;d;a;a;settime:10:59:58;clock:3000", ["T-70 T.hits.hour: chime_quiet → chime_beep"],
     ["chime_beep"], []),
    ("T-71", "a;a;a;d;a;a;settime:10:59:58;clock:3000;advance:2000", ["T-71 timeout.chime_beep: chime_beep → chime_quiet"],
     ["chime_quiet"], []),
    ("T-72", "+b", ["T-72 b.down: light_off → light_on"], ["light_on"], []),
    ("T-73", "+b;-b", ["T-73 b.up: light_on → light_off"], ["light_off"], []),
    ("T-74", "+b", ["T-74 b.down: bt_00 → bt_10"], ["bt_10"], []),
    ("T-75", "+d", ["T-75 d.down: bt_00 → bt_01"], ["bt_01"], []),
    ("T-76", "+b;+d", ["T-76 d.down: bt_10 → bt_beep"], ["bt_beep_on", "time"], ["T-20"]),
    ("T-77", "+d;+b", ["T-77 b.down: bt_01 → bt_beep"], ["bt_beep_on", "date"], []),
    ("T-78", "+b;-b", ["T-78 b.up: bt_10 → bt_00"], ["bt_00"], []),
    ("T-79", "+d;-d", ["T-79 d.up: bt_01 → bt_00"], ["bt_00"], []),
    ("T-80", "+d;+b;-d", ["T-80 d.up: bt_beep → bt_10"], ["bt_10"], []),
    ("T-81", "+d;+b;-b", ["T-81 b.up: bt_beep → bt_01"], ["bt_01"], []),
    ("T-82", "+d;+b;+a", ["T-82 a.down: bt_beep_on → bt_beep_off"], ["bt_beep_off", "date"], []),
    ("T-83", "+d;+b;+a;-a", ["T-83 a.up: bt_beep_off → bt_beep_on"], ["bt_beep_on"], []),
    # scenarios
    ("S-1 b in update: light on and exit (p. 247)", UPD + ";c;c;c;+b",
     ["T-28 b.down: update → time", "T-72 b.down: light_off → light_on"], ["time", "light_on"], []),
    ("S-2 mon, d then b held (p. 250)", UPD + ";c;c;c;c;+d;+b",
     ["◀ model.increment field=update_mon", "T-28 b.down: update → time", "T-77 b.down: bt_01 → bt_beep"],
     ["time", "bt_beep_on", "light_on"], []),
    ("S-3 battery dies in update2, re-insert", "a;d;a;d;c;c;battery.weakens;battery.dies;battery.inserted;a;a",
     ["T-02 battery.dies: power_blink → dead", "T-11 a.down: alarm1 → alarm2"],
     ["alarm2_off", "alarm2_disabled", "alarm1_disabled"], ["alarm2 → alarm2_H"]),
    ("S-4 Fig. 32 flag off", "set:harelAnomalies:false;+d;+b;+a", ["T-77 b.down: bt_01 → bt_beep"],
     ["bt_beep_on"], ["T-82"]),
]


APP_FAILURES = []   # rows whose app run produced no log, with the app's own output


def run(row):
    rid, steps, expect, active, forbid = row
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "log.txt")
        script = "battery.inserted;wait:150;" + ";".join(
            s + ";wait:120" for s in steps.split(";") if s) + f";wait:200;export:{out}"
        # --no-gui: the walk checks the statechart, input path and event log, not
        # rendering, so the app runs without a window or platform plugin. (With its
        # window under the offscreen platform it crashed at start-up on macOS CI.)
        env = dict(os.environ)
        app_status = ""
        try:
            proc = subprocess.run(CMD + ["--no-gui", "--time", "1987-06-01T10:08:30", "--manual-time", "--exit",
                                   "--grab-delay", "50", "--script", script], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
            app_output = proc.stdout.decode("utf-8", errors="replace")
            app_status = f"exit code {proc.returncode}"
        except subprocess.TimeoutExpired as e:
            app_output = (e.stdout or b"").decode("utf-8", errors="replace")
            app_status = "timed out after 60 s"
        text = pathlib.Path(out).read_text(encoding="utf-8") if os.path.exists(out) else ""
    lines = text.splitlines()
    if not lines:
        # The app produced no event log at all: say why instead of only listing
        # every expectation as missing.
        tail = [l for l in app_output.splitlines() if l.strip()][-8:]
        APP_FAILURES.append(f"{rid}: app {app_status}; output:\n    " + "\n    ".join(tail or ["(none)"]))
    act = next((l[len("# active: "):].split() for l in lines if l.startswith("# active: ")), [])
    seen = [e for e in expect if any(e in l for l in lines)]
    missing = [e for e in expect if e not in seen]
    missing_states = [s for s in active if s not in act]
    forbidden = [f for f in forbid if any(f in l for l in lines)]
    ok = not missing and not missing_states and not forbidden and bool(lines)
    observed = []
    for e in expect:
        observed.append(("`" + e + "`") if e in seen else f"**missing** `{e}`")
    observed.append("active ⊇ {" + ", ".join(s if s in act else f"**{s}?**" for s in active) + "}")
    if forbid:
        observed.append("absent: " + ", ".join(("**present** " if f in forbidden else "") + f"`{f}`" for f in forbid))
    return rid, steps, expect, active, forbid, ok, observed


def split_command(cmd):
    """Split --cmd like the platform's shell would. POSIX rules eat the backslashes
    of Windows paths, so on Windows split with posix=False and drop the quotes
    around each token itself (posix=False keeps them, and subprocess would pass
    them on as part of the program name)."""
    if os.name != "nt":
        return shlex.split(cmd)
    return [t[1:-1] if len(t) >= 2 and t[0] == t[-1] and t[0] in "\"'" else t
            for t in shlex.split(cmd, posix=False)]


def main():
    global CMD
    ap = argparse.ArgumentParser()
    ap.add_argument("build_dir", nargs="?", default="build")
    src = ap.add_mutually_exclusive_group()
    src.add_argument("--bin", help="the watch-me executable")
    src.add_argument("--cmd", help="start command of another host (implies --no-doc; not with --check)")
    doc_mode = ap.add_mutually_exclusive_group()
    doc_mode.add_argument("--check", action="store_true", help="fail if docs/VERIFICATION.md is not current")
    doc_mode.add_argument("--no-doc", action="store_true", help="neither write nor check docs/VERIFICATION.md")
    args = ap.parse_args()
    if args.cmd and args.check:
        ap.error("--check needs the C++ host (--bin); --cmd never writes or checks docs/VERIFICATION.md")
    if args.cmd:
        CMD = split_command(args.cmd)
        if not CMD:
            ap.error("--cmd is empty")
        if shutil.which(CMD[0]) is None and not pathlib.Path(CMD[0]).is_file():
            sys.exit(f"host command not found: {CMD[0]}")
        args.no_doc = True
    else:
        binary = pathlib.Path(args.bin) if args.bin else ROOT / args.build_dir / "watch-me"
        if not binary.exists():
            sys.exit(f"build the app first: {binary}")
        CMD = [str(binary)]
    with cf.ThreadPoolExecutor(max_workers=4) as ex:
        results = list(ex.map(run, ROWS))
    spec = (ROOT / "docs/STATECHART-SPEC.md").read_text(encoding="utf-8")
    spec_rows = sorted(set(re.findall(r"^\|\s*(T-\d+)\s*\|", spec, re.M)), key=lambda t: int(t[2:]))
    walked = {r[0] for r in results}
    uncovered = [r for r in spec_rows if r not in walked]
    fails = [r for r in results if not r[5]]

    md = ["# Verification — every spec row walked in the running app (P8)", "",
          "Generated by `tools/verify.py`. For each row the **application binary** is started",
          "without its window (`--no-gui`) with the battery inserted at 1987-06-01 10:08:30, driven through its normal",
          "input path (`ButtonRouter` → `WatchMachine`; clock events from `WatchModel`), and the",
          "app's own event log (`TransitionTracer`, the same log shown in the UI) is exported and",
          "checked. Steps: `a` = click, `+a`/`-a` = hold/release, `advance:ms` = move the",
          "statechart's timeout clock (so \"2 min in date\" is `advance:120000`, exact and",
          "independent of machine speed), `clock:ms` = move the watch's time T, `alarm:n:HH:MM`,",
          "`jump:n:s` = T := Tn − s, `settime:H:M:S`. The app runs with `--manual-time`, so",
          "only these steps move time. `◀` lines are outgoing events (D-11).", "",
          f"Result: **{len(results) - len(fails)} / {len(results)} ✓**"
          + (f", spec rows not walked: {', '.join(uncovered)}" if uncovered else ", every spec row walked") + ".", "",
          "Alongside this run, the spec was re-read against Fig. 31 (PDF pp. 21–22 at 400 dpi)",
          "and the generated statechart view (`docs/screenshots/app-*.png`) was compared with",
          "the figure region by region: same components, same nesting, same defaults and",
          "history connectors. Known, documented differences are listed at the end.", "",
          "| Row | Steps | Expected | Observed | |", "|---|---|---|---|---|"]
    for rid, steps, expect, active, forbid, ok, observed in results:
        exp = "<br>".join(f"`{e}`" for e in expect) + ("<br>in: " + ", ".join(active) if active else "")
        if forbid:
            exp += "<br>not: " + ", ".join(f"`{f}`" for f in forbid)
        md.append(f"| {rid} | `{steps or '—'}` | {exp} | {'<br>'.join(observed)} | {'✓' if ok else '✗'} |")
    md += ["", "## Known differences from the paper (all explained in DESIGN-NOTES)", "",
           "- `clh(main*)` emulated with history-valid flags (D-2); verified by S-3.",
           "- `d → H` in the update states implemented as targetless increments (D-4); T-27/T-33/T-38.",
           "- All three 2-minute returns restart on any button (D-3); T-47.",
           "- T-82 carries `!In('time')` to give T-10 priority on Qt 6.4 (D-12).",
           "- Fig. 32 behind `harelAnomalies` (on by default); S-4 checks it off.",
           "",
           "Visible in the statechart view when compared with Fig. 31:", "",
           "- `update`, `update1`, `update2` show no H connector: their `d` arrows to H are the",
           "  targetless increments of D-4, so SCXML needs no history pseudostate there.",
           "- `beep-test.beep` shows the Fig. 32 refinement `on | off` (not part of Fig. 31).",
           "- `out` shows a default arrow on `alarm1`: SCXML requires an initial child; the",
           "  paper never enters `out` by default (D-17).",
           "- The unnamed components are named `timedate` (in `regular`) and `nonzero` (in",
           "  `stopwatch`) because SCXML states need ids (D-17).", ""]
    if APP_FAILURES:
        print(f"FAIL the app produced no event log in {len(APP_FAILURES)} run(s); first ones:")
        for a in APP_FAILURES[:3]:
            print("  " + a)
    doc = ROOT / "docs/VERIFICATION.md"
    text = "\n".join(md)
    stale = False
    if args.no_doc:
        pass
    elif args.check:
        committed = doc.read_text(encoding="utf-8") if doc.exists() else ""
        if committed != text:
            stale = True
            print("FAIL docs/VERIFICATION.md is not current; run tools/verify.py and commit it:")
            sys.stdout.writelines(difflib.unified_diff(committed.splitlines(True), text.splitlines(True),
                                                       "committed", "generated"))
            print()
    else:
        doc.write_text(text, encoding="utf-8")
    print(f"{len(results) - len(fails)}/{len(results)} ok; uncovered: {uncovered}")
    for f in fails:
        print("FAIL", f[0], f[6])
    return 1 if fails or uncovered or stale else 0


if __name__ == "__main__":
    sys.exit(main())
