#!/usr/bin/env python3
"""watch-me, Python host: Harel's 1987 watch statechart, executable.

Loads shared/statechart/watch.scxml with Qt SCXML at run time and shared/qml/Main.qml
from disk, and provides the host objects of docs/HOST-CONTRACT.md.
Start from anywhere: python3 python/main.py [options]
"""
from __future__ import annotations

import argparse
import json
import sys
import traceback
from collections import deque
from datetime import datetime
from pathlib import Path

from PySide6.QtCore import QCoreApplication, QLoggingCategory, QObject, QTimer, QUrl
from PySide6.QtScxml import QScxmlStateMachine

from watchme.button_router import ButtonRouter
from watchme.extras import Beeper, SpecTags
from watchme.scxml_tree import ScxmlTreeModel
from watchme.timeout_service import TimeoutService
from watchme.tracer import TransitionTracer
from watchme.watch_model import WatchModel

ROOT = Path(__file__).resolve().parent.parent       # shared paths do not depend on the cwd
SCXML = ROOT / "shared" / "statechart" / "watch.scxml"
LAYOUT = ROOT / "shared" / "statechart" / "layout.json"
MAIN_QML = ROOT / "shared" / "qml" / "Main.qml"
SPEC = ROOT / "docs" / "STATECHART-SPEC.md"
VALUE_OPTIONS = ("--script", "--grab", "--grab-delay", "--time", "--size")


def parse_args(argv: list[str]) -> argparse.Namespace:
    ap = argparse.ArgumentParser(prog="watch-me", description="Harel's 1987 watch statechart, executable.")
    ap.add_argument("--no-gui", action="store_true", help="No window: statechart, input, log and --script only.")
    ap.add_argument("--auto-insert", action="store_true", help="Insert the battery at start-up.")
    ap.add_argument("--time", help="Clock value set by model.reset (ISO 8601, e.g. 1987-06-01T10:08:30).")
    ap.add_argument("--manual-time", action="store_true", help="Only script steps move the clocks.")
    ap.add_argument("--script", default="", help="Semicolon separated steps run after start-up.")
    ap.add_argument("--exit", action="store_true", help="Quit when the --script has run.")
    ap.add_argument("--grab", default="", help="Save a screenshot of the window to FILE and quit.")
    ap.add_argument("--grab-delay", type=int, default=800, help="Delay before --grab / --exit (ms).")
    ap.add_argument("--size", help="Window size, e.g. 1920x1080.")
    # QCommandLineParser takes "--script -a"; argparse would read "-a" as an option.
    fixed, i = [], 0
    while i < len(argv):
        if argv[i] in VALUE_OPTIONS and i + 1 < len(argv):
            fixed.append(f"{argv[i]}={argv[i + 1]}")
            i += 2
        else:
            fixed.append(argv[i])
            i += 1
    return ap.parse_args(fixed)


def to_int(s: str) -> int:
    """QString::toInt: 0 if it is not a number."""
    try:
        return int(s)
    except ValueError:
        return 0


def to_float(s: str) -> float:
    try:
        return float(s)
    except ValueError:
        return 0.0


def main() -> int:
    args = parse_args(sys.argv[1:])
    if args.no_gui and (args.grab or args.size):
        print("watch-me: --grab and --size need the window; they cannot be used with --no-gui.", file=sys.stderr)
        return 1
    reset_time = None
    if args.time:
        try:
            reset_time = datetime.fromisoformat(args.time).replace(tzinfo=None)
        except ValueError:
            print(f"watch-me: --time {args.time!r} is not an ISO date-time; using the host clock", file=sys.stderr)

    if args.no_gui:
        app = QCoreApplication(sys.argv)
    else:
        from PySide6.QtGui import QGuiApplication
        app = QGuiApplication(sys.argv)
    app.setApplicationName("watch-me")
    # <log> elements are spec-row markers for the tracer, not console output.
    QLoggingCategory.setFilterRules("scxml.statemachine.debug=false")

    # https://doc.qt.io/qt-6/qscxmlstatemachine.html#fromFile
    machine = QScxmlStateMachine.fromFile(str(SCXML))
    errors = machine.parseErrors() if machine else []
    if machine is None or errors:
        for e in errors:
            print(f"watch-me: {e.toString()}", file=sys.stderr)
        return 2
    if machine.dataModel() is None:
        print("watch-me: Qt SCXML's ECMAScript datamodel plugin was not found; "
              "the installation is incomplete.", file=sys.stderr)
        return 2
    machine.setParent(app)

    tree = ScxmlTreeModel(app)
    tree.load(str(SCXML))
    spec_tags = SpecTags(app)
    spec_tags.load(SPEC)
    tracer = TransitionTracer(machine, tree, app)
    watch = WatchModel(machine, app)
    timeouts = TimeoutService(machine, app)
    if not args.manual_time:
        watch.set_running(True)
        timeouts.set_running(True)
    watch.set_reset_time(reset_time)
    buttons = ButtonRouter(machine, tracer, app)
    beeper = Beeper(app)
    watch.shortBeep.connect(beeper.shortBeep)

    engine = None
    if not args.no_gui:
        from PySide6.QtQml import QQmlApplicationEngine
        import PySide6.QtQuick  # noqa: F401  (rootObjects() then returns QQuickWindow, not QWindow)
        engine = QQmlApplicationEngine()
        # Main.qml's required properties: the host contract (docs/HOST-CONTRACT.md §3).
        # https://doc.qt.io/qt-6/qqmlapplicationengine.html#setInitialProperties
        engine.setInitialProperties({
            "tree": tree, "tracer": tracer, "buttons": buttons, "watch": watch,
            "beeper": beeper, "specTags": spec_tags,
            "appLayout": json.loads(LAYOUT.read_text(encoding="utf-8")),
        })
        engine.load(QUrl.fromLocalFile(str(MAIN_QML)))
        if not engine.rootObjects():
            print("watch-me: could not load Main.qml", file=sys.stderr)
            return 1
        if args.size:
            wh = args.size.split("x")
            if len(wh) == 2:
                engine.rootObjects()[0].resize(to_int(wh[0]), to_int(wh[1]))

    machine.start()
    if args.auto_insert:
        QTimer.singleShot(0, lambda: buttons.submit("battery.inserted"))

    steps = deque(s for s in args.script.split(";") if s)

    def grab_and_quit() -> None:
        try:
            window = engine.rootObjects()[0]       # a QQuickWindow (PySide6.QtQuick is imported)
            if not window.grabWindow().save(args.grab):
                print(f"could not save {args.grab}", file=sys.stderr)
        finally:
            app.quit()

    def run_step(s: str) -> int:
        """Run one script step (HOST-CONTRACT §5.1); returns the gap to the next one."""
        parts = s.split(":")
        head, n = parts[0], len(parts)
        if s.startswith("wait:"):
            return to_int(s[5:])
        if head == "alarm" and n == 4:
            watch.setAlarm(to_int(parts[1]), to_int(parts[2]) * 60 + to_int(parts[3]))
        elif head == "jump" and n == 3:
            watch.jumpBeforeAlarm(to_int(parts[1]), to_int(parts[2]))
        elif head == "settime" and n == 4:
            watch.setTime(to_int(parts[1]), to_int(parts[2]), to_int(parts[3]))
        elif head == "advance" and n == 2:
            timeouts.advance(to_int(parts[1]))
        elif head == "clock" and n == 2:
            watch.advance(to_int(parts[1]))
        elif head == "speed" and n == 2:
            watch.set_speed(to_float(parts[1]))
        elif s.startswith("export:"):
            # the rest of the step: a Windows path has a ':' of its own (C:\...)
            if not tracer.exportLog(s[7:]):
                print(f"could not export the log to {s[7:]}", file=sys.stderr)
        elif head == "select" and n == 2:
            if engine is None:
                print("watch-me: 'select:' needs the window; ignored with --no-gui", file=sys.stderr)
            else:
                # Keep the window in a variable: with a temporary, PySide6 6.8.3 invalidates
                # the wrapper findChild returns ("Internal C++ object already deleted").
                window = engine.rootObjects()[0]
                view = window.findChild(QObject, "statechartView")
                if view is not None:
                    view.setProperty("selected", parts[1])
        elif head == "set" and n == 3:
            v = parts[2]
            buttons.setDataValue(parts[1], True if v == "true" else False if v == "false" else to_float(v))
        elif len(s) == 1:
            buttons.press(s)
            QTimer.singleShot(30, lambda: buttons.release(s))
        elif len(s) == 2 and s[0] == "+":
            buttons.press(s[1])
        elif len(s) == 2 and s[0] == "-":
            buttons.release(s[1])
        else:
            buttons.submit(s)
        return 60

    def run_next() -> None:
        if not steps:
            if args.grab:
                QTimer.singleShot(args.grab_delay, grab_and_quit)
            elif args.exit:
                QTimer.singleShot(args.grab_delay, app.quit)
            return
        step = steps.popleft().strip()
        try:
            gap = run_step(step)
        except Exception:               # a Python error must not stall the script (and --exit)
            traceback.print_exc()
            print(f"watch-me: step {step!r} failed", file=sys.stderr)
            gap = 60
        QTimer.singleShot(gap, run_next)

    QTimer.singleShot(200, run_next)
    status = app.exec()
    del engine                  # destroy the QML scene before the objects it uses
    return status


if __name__ == "__main__":
    sys.exit(main())
