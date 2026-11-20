# watch-me — Python host (PySide6)

The glue of `docs/HOST-CONTRACT.md` in Python. It loads the shared statechart
(`shared/statechart/watch.scxml`) at run time with Qt SCXML and the shared UI
(`shared/qml/Main.qml`) from disk; nothing is generated or compiled.

| File | Contract part |
|---|---|
| `main.py` | command line, start-up, `--script` steps (§5) |
| `watchme/watch_model.py` | `WatchModel` (clock, alarms, stopwatch) |
| `watchme/timeout_service.py` | `TimeoutService` (§5.2) |
| `watchme/button_router.py` | `ButtonRouter` |
| `watchme/tracer.py` | `TransitionTracer`, the event log (§4) |
| `watchme/scxml_tree.py` | `ScxmlTreeModel` (`xml.etree`, not Qt SCXML) |
| `watchme/extras.py` | `SpecTags`, `Beeper` |
| `watchme/scxml_events.py` | `connectToEvent` from Python (see below) |

## Setup

Python 3.10 or newer. From the repository root:

```sh
python3 -m venv python/.venv
. python/.venv/bin/activate            # Windows: python\.venv\Scripts\activate
pip install -r python/requirements.txt  # PySide6 6.8.3, which bundles Qt 6.8.3 incl. Qt SCXML
```

On a bare Linux machine Qt also needs some system libraries (the list is in
`.github/workflows/python.yml`, e.g. `libegl1 libgl1 libxkbcommon0 libxcb-cursor0
libfontconfig1`).

## Run

```sh
python3 python/main.py --auto-insert
python3 python/main.py --no-gui --manual-time --exit --script "battery.inserted;a;export:/tmp/log.txt"
QT_QPA_PLATFORM=offscreen python3 python/main.py --time 1987-06-01T10:08:30 \
    --script "battery.inserted;a;a;a;a;b" --grab /tmp/py.png
```

The options are those of the contract (§5); `--help` lists them. Paths to
`shared/` and `docs/` are resolved from this file, so the current directory does
not matter.

## Verify

```sh
python3 tools/verify.py --cmd "python3 python/main.py"   # 68/68 ok; uncovered: []
```

The exported logs of all 68 walk runs are byte-identical to the C++ host's.

## Notes for the comparison

- **`connectToEvent`.** PySide6 binds only the string-based overload
  `connectToEvent(spec, receiver, SLOT("method(QScxmlEvent)"))`, not the functor
  one, and `QScxmlStateMachine::EventConnection` is not available. So each
  callback gets a tiny `QObject` with a `@Slot(QScxmlEvent)`
  (`watchme/scxml_events.py`). With that, the events arrive exactly as in C++:
  `model.reset` during the macrostep that sends it (D-21), in order.
- **QVariant conversion.** Event data arrives as a Python `dict` (keys in
  QVariantMap order, i.e. sorted), but an event without `<param>` has data `""`
  (a `str`), not an empty map: `event_data()` normalises that. Data passed to
  `submitEvent(name, dict)` converts back without help (`{"gen": gen}` keeps
  the statechart's number, so `_event.data.gen == tg_x` holds). Whole ECMAScript
  numbers came back as `int`, others would be `float`, so the `◀ key=value` text
  goes through a small `QVariant::toString` imitation (`2.0` → `2`, `True` → `true`).
- **The `log` signal** is an ordinary signal: `machine.log.connect(callback)`.
  It arrives after the macrostep as in C++, so the attribution of §4.3 is a
  straight port.
- **Event draining** (§5.2) is `QCoreApplication.sendPostedEvents(None,
  QEvent.Type.MetaCall)`, as in C++; it works unchanged.
- **Properties for QML** are `Property("QVariantMap" | "QStringList" | int | …,
  getter, notify=…)` and `@Slot(..., result=...)`. Python dicts and lists convert
  to JS objects and arrays; `appLayout` is passed as a plain `dict` to
  `setInitialProperties`.
- **PySide6 traps found on the way:**
  - `engine.rootObjects()[0]` is typed `QWindow` (no `grabWindow`) unless
    `PySide6.QtQuick` has been imported; then it is a `QQuickWindow`.
  - `engine.rootObjects()[0].findChild(...)` on a temporary returns a wrapper
    that is already invalid ("Internal C++ object already deleted"); keep the
    window in a variable first.
  - An exception in a `QTimer.singleShot` callback is printed but does not stop
    the event loop, so a failing step stalled the script and `--exit` never came:
    `run_next` catches and reports step errors.
- **Rounding.** `timer.start{secs}` uses `int(x * 1000 + 0.5)` (`qRound64`), not
  `round()` (banker's rounding). All durations in the chart are whole seconds, so
  this does not show.
- **Small departures from the C++ host** (none from the contract):
  - `--time` that is not ISO 8601 prints a warning and uses the host clock (C++
    silently uses the host clock); `datetime.fromisoformat` is the parser.
  - Argument parsing is `argparse`. `QCommandLineParser` accepts `--script -a`;
    argparse would read `-a` as an option, so `main.py` joins value options to
    `--opt=value` before parsing.
  - `exportLog` always writes `\n`; the C++ host opens the file in text mode,
    which writes `\r\n` on Windows (the contract says `\n`).
  - `stopwatchCentis` is a 32-bit `int` property for QML (C++: `qint64`); it
    overflows after about 248 days of running stopwatch.
  - The `Beeper` uses `QSoundEffect` from the PySide6 wheel, loaded on first
    enable; `available` is false if the wheel lacks QtMultimedia.
