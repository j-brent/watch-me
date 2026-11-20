# watch-me: the QML/JavaScript host

This host has no compiled code. It is plain JavaScript inside QML, run by Qt's
`qml` runtime tool. It loads the shared statechart (`shared/statechart/watch.scxml`)
at run time with Qt SCXML's `StateMachineLoader`, and it creates the shared UI
(`shared/qml/Main.qml`) with its own QML objects as the required properties. It
follows [`docs/HOST-CONTRACT.md`](../docs/HOST-CONTRACT.md). The C++ host in
`cpp/src/` is the reference, and each file here ports one of its classes.

| File | Contract object / role |
|---|---|
| `watch-me-qmljs` | Launcher (bash): finds `qml`, sets the environment, picks `--apptype` |
| `Host.qml` | `main.cpp`: command line, start-up, `--script`, `--grab` |
| `WatchModel.qml` | `watch`: clock T, alarms, stopwatch, `T.hits.*`, `host.alarms` |
| `TimeoutService.qml` | the "n sec in X" clock: `timer.start`/`timer.cancel` → `timeout.<id>` |
| `ButtonRouter.qml` | `buttons`: press/release/submit, `host.data` cache, `host.anomalies` |
| `TransitionTracer.qml` | `tracer`: active set, flashes, event log, export |
| `ScxmlTreeModel.qml` + `scxml.mjs` | `tree`: the SCXML parsed by a small XML tokenizer in JS |
| `SpecTags.qml` | `specTags`: `[P|F|A]` per row from `docs/STATECHART-SPEC.md` |
| `Beeper.qml` | `beeper`: no audio (`available: false`) |
| `files.mjs` | file I/O through `XMLHttpRequest` on `file:` URLs |

## Run

You need Qt 6.8 with the Qt SCXML module. The launcher finds the `qml` tool in
this order: `$QT_QML_BIN` (the path to the tool itself), `$QTDIR/bin/qml`,
`$QT_ROOT_DIR/bin/qml` (set by `jurplel/install-qt-action`), then `PATH`.

```sh
QTDIR=/opt/Qt/6.8.3/gcc_64 qmljs/watch-me-qmljs --auto-insert
QTDIR=/opt/Qt/6.8.3/gcc_64 QT_QPA_PLATFORM=offscreen qmljs/watch-me-qmljs \
    --time 1987-06-01T10:08:30 --script "battery.inserted;a;a;a;a;b" --grab /tmp/js.png
```

The options and script steps are the contract's (§5), plus `--help`. Without the
launcher, run:

```sh
QML_XHR_ALLOW_FILE_READ=1 QML_XHR_ALLOW_FILE_WRITE=1 \
QT_LOGGING_RULES="scxml.statemachine.debug=false" \
qml --apptype gui qmljs/Host.qml -- --cwd "$PWD" [options]       # --apptype core with --no-gui
```

## Verify

```sh
QTDIR=/opt/Qt/6.8.3/gcc_64 python3 tools/verify.py --cmd "qmljs/watch-me-qmljs"
# 68/68 ok; uncovered: []
```

The exported logs of all 68 runs are byte-for-byte the same as the C++ host's.
CI runs the walk in `.github/workflows/qmljs.yml`.

## Notes for the comparison

**Environment the host needs** (the launcher sets all of these):

- `QML_XHR_ALLOW_FILE_READ=1`: `XMLHttpRequest` is the only way plain QML can
  read a file. The host reads `watch.scxml`, `layout.json` and `STATECHART-SPEC.md`.
- `QML_XHR_ALLOW_FILE_WRITE=1`: the only way to write one (`PUT`). The host uses
  it for `exportLog`.
- `QT_LOGGING_RULES=scxml.statemachine.debug=false`: keeps `<log>` off the
  console. The C++ host sets this rule in code; QML cannot.
- `--apptype core` for `--no-gui`: a core application, with no window and no
  platform plugin. `Host.qml` imports only `QtQml` and `QtScxml`. `Main.qml`
  (QtQuick) is created only in GUI mode, with `Qt.createComponent(...).createObject(null, {…})`.

**What QML/JS cannot do that compiled hosts can, and the workarounds:**

- **Read or write the SCXML datamodel.** Qt does not expose
  `QScxmlDataModel::scxmlProperty` to QML. The event-only boundary (D-24:
  `host.alarms`, `host.anomalies`, `host.data`) exists for this reason, and with
  it this host needs nothing else from the datamodel.
- **`connectToEvent`** is a C++ template, not callable from QML. This host uses
  `EventConnection` (`events: ["model.*", …]`) instead, with the same timing.
  `log`, `reachedStableState`, `runningChanged` and `initializedChanged` are
  received with `Connections`.
- **Draining the machine's queue.** The C++ `TimeoutService::advance` calls
  `QCoreApplication::sendPostedEvents` so that the machine handles a `timeout.<id>`
  before the next deadline is looked for (§5.2, D-21). JavaScript cannot run the
  event loop. Here `advance` is asynchronous. It waits until the machine has
  settled, which means two `Qt.callLater` turns in a row with no
  `reachedStableState`, `log` or `timer.*`. Then it fires the earliest deadline,
  waits again, and repeats. A call made while one is running is added to a
  backlog instead of being dropped. Script steps are 60–120 ms apart, and a
  step's chain of timeouts settles within a few event-loop turns.
- **Synchronous file writes.** Qt 6.8's `XMLHttpRequest` `PUT` to a `file:` URL
  ends with status 0 whether or not the write worked. A synchronous `PUT`
  creates the file but writes no body. So `exportLog` writes asynchronously,
  reads the file back to check it, and then emits `exported(path, ok)`. A failed
  write also prints `could not export the log to …`. The host waits for pending
  writes before it quits. Reads use synchronous `GET`, which works.
- **XML parsing.** `responseXML` is a reduced DOM that drops comments, and the
  `<!-- T-xx -->` fallback needs them. `scxml.mjs` is a small tokenizer (about 60 lines).
- **The working directory.** QML cannot read it, and `Qt.resolvedUrl` resolves
  against the QML file. The launcher passes `--cwd "$PWD"`, and relative
  `export:`/`--grab` paths resolve against it. The sim panel's default
  `watch-me-events.txt` resolves the same way.
- **Window screenshots.** `QQuickWindow::grabWindow` is not invokable from QML.
  `--grab` uses `Item.grabToImage` on the window's root item. That grab does not
  paint the window's clear colour, so the host first puts a `Rectangle` in
  `Main.color` behind the content.
- **`findChild`.** QML has none, so `select:` walks `children` from the content
  item to find `statechartView`.
- **Audio.** Plain QML cannot synthesise a tone. The C++ host writes a WAV into
  memory and plays it with Qt Multimedia. Here `Beeper.available` is `false`, so
  the sim panel's "sound" box is disabled and `shortBeep()` does nothing.
- **Exit codes before the event loop.** `Qt.exit()` does nothing before
  `exec()`, so start-up failures (the machine cannot load, `--no-gui` with
  `--grab`, an unknown option) defer it with `Qt.callLater`. A missing or
  unparsable `watch.scxml` exits with 2, after `StateMachineLoader`'s own
  message. A missing ECMAScript datamodel also exits with 2. Bad options exit with 1.

**Where this host differs from the contract:**

- `tracer.exportLog(path)` returns `true` when the write *started*, not when it
  finished (see above). It returns `false` only for an empty path. The final
  result comes later through `exported(path, ok)`. The sim panel therefore shows
  "saved" before the write has finished, and also for a directory that does not
  exist (the warning is printed on the console).
- `TimeoutService.advance` is asynchronous (see above). Its order of events is
  the contract's, but `now` reaches the end of the step one or more event-loop
  turns after the call returns.
- `--cwd DIR` is an extra option that the launcher adds.
