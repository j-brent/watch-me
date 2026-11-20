# qmlts — the TypeScript host

The glue of [docs/HOST-CONTRACT.md](../docs/HOST-CONTRACT.md) written in TypeScript,
compiled by `tsc` to ES modules (`.mjs`) that thin QML components import, and run
by Qt's `qml` tool. No C++ is compiled. The statechart
(`shared/statechart/watch.scxml`) runs in Qt SCXML through `StateMachineLoader`; the
UI is `shared/qml/Main.qml`, unchanged. `cpp/` is the reference; this host follows
it and produces byte-identical event logs on all 68 rows of the walk.

| Path | What |
|---|---|
| `src/*.mts` | All logic, strict TypeScript: `watchModel` (clock, alarms, stopwatch), `timeoutService` (§5.2), `buttonRouter`, `tracer` (log, attribution §4.3, export), `scxmlTree` + `xml` (an own XML reader), `specTags`, `cli` (options and script steps), `script` (runner), `machineGate` (submit + "wait until the machine has settled"), `files` (XHR), `wallclock` (zone-less date arithmetic), `ports` (the Qt interfaces the logic calls) |
| `qml/*.qml` | One thin wrapper per host object, exposing exactly the members of contract §3 and forwarding to `build/*.mjs`; `Host.qml` is the root |
| `watch-me-qmlts` | Launcher: sets the environment, runs `qml [--apptype core] qml/Host.qml -- <args>` |

## Build, run, verify

Needs Node ≥ 18 (CI uses 22) and Qt 6.8 with the Qt SCXML module.

```sh
(cd qmlts && npm ci && npm run build)        # tsc → qmlts/build/*.mjs (not committed)
qmlts/watch-me-qmlts --auto-insert           # the app
qmlts/watch-me-qmlts --no-gui --time 1987-06-01T10:08:30 --manual-time --exit \
    --script "battery.inserted;a;export:/tmp/log.txt"
python3 tools/verify.py --cmd "qmlts/watch-me-qmlts"   # → 68/68 ok; uncovered: []
(cd qmlts && npx tsc --noEmit -p .)          # type check only
```

The launcher finds `qml` in `$QT_QML_BIN` (path of the tool), else on `PATH`, else in
`$QT_ROOT_DIR/bin`. It does not build: after editing `src/`, run `npm run build`.
It sets:

| Variable | Why |
|---|---|
| `QML_XHR_ALLOW_FILE_READ=1` | QML's `XMLHttpRequest` reads `watch.scxml`, `STATECHART-SPEC.md` and `layout.json` |
| `QML_XHR_ALLOW_FILE_WRITE=1` | the `export:` step and the sim panel's *export* write the log with an XHR `PUT` |
| `QT_LOGGING_RULES=scxml.statemachine.debug=false` | the `<log>` elements are row markers, not console output (C++ sets this in `main.cpp`) |

and passes `--host-cwd "$PWD"` ahead of your arguments, so that a relative export
path resolves against your directory (QML has no `cwd`). `--no-gui` selects
`qml --apptype core`: a `QCoreApplication`, no window, no platform plugin.
Options are those of contract §5; `--help` lists them.

Audio: `Beeper.available` is `false`. QML alone cannot synthesise the tone (the C++
host writes a WAV and plays it with Qt Multimedia); the "sound" box is disabled.

## Notes for the comparison

**What TypeScript bought.** Fewer bugs found than hoped: the first compile was clean
and the first conformance run passed 68/68 with byte-identical logs, so the checker
caught nothing a run would have. What it did do:
- `lib: ["ES2016"]` + `target: "ES2016"` keep the code inside what Qt's engine
  parses (below), and tsc lowers object spread, `?.`, `??` and parameter properties for it.
  A `/s` regex flag (ES2018) is a compile error under this target; V4 would have
  rejected the module at load time with a less useful message.
- The script steps are a discriminated union with an exhaustive `switch`
  (`never` check), so a new step kind cannot be parsed and then silently ignored.
- `noUncheckedIndexedAccess` and `exactOptionalPropertyTypes` forced every
  `regex[1]`, `args[i]` and map lookup to say what happens when it is missing,
  which is where a hand port of `QStringList::value()` defaults usually drifts.
- Types say which number is a wall-clock moment (`Wall`, branded) and which a
  duration.

What it cannot check is the boundary, and that is where all the surprises were:
the Qt objects are hand-declared interfaces (`src/ports.mts`, `machineGate.mts`,
`qml-globals.d.ts`) and the QML wrappers are untyped JavaScript. Facts the types
did not and could not tell: `QScxmlEvent.data` is `""`, not `{}`, for an event
without data; the content item of a window created through QML still cannot
`grabToImage`; a synchronous XHR `PUT` creates an empty file.

**ES modules in Qt 6.8's engine (V4).** `import "../build/x.mjs" as X` in QML and
`import { y } from "./y.mjs"` between modules both work, relative to the importing
file; a module is evaluated once per engine and shared by every importer (so all
state lives in class instances, one per QML wrapper). Missing, found by probing:
`async`/`await`, object spread `{...o}`, class fields (`x = 1;`), private `#x`
(all "Unexpected token" at load time, so the whole module is "unavailable").
Present: classes, generators, `Map`/`Set`, `for…of`, sticky regexes, Promises
(resolved after the current QML handler). Inside a module there is no `Qt`, no
`setTimeout`, no `Timer` and no file API — only what QML passes in, which is why
timers, the machine and the clock are injected (`ports.mts`). tsc's
`.mts → .mjs` output works without a bundler.

**The build step.** One more moving part than `qmljs/`: `npm ci && npm run build`
before the first run and after every edit, a `node_modules/` (TypeScript only, pinned
to 5.9.3), and stale `build/*.mjs` are easy to run by mistake — the launcher only
checks that `build/` exists, not that it is current. CI type-checks and builds
before the walk. Compile time is about a second.

**Where the contract could not be followed exactly.**
- §5.2 step 1 and "let the machine process it": JavaScript cannot drain Qt SCXML's
  queue synchronously (C++ uses `QCoreApplication::sendPostedEvents`).
  `TimeoutService.advance` is asynchronous: after each `timeout.<id>` it waits for
  the machine to settle and then looks for the next deadline. "Settled" is
  `MachineGate.settle`: every submit is counted, and the next `reachedStableState`
  means all of them were processed (observed: one posted call processes the whole
  external queue, including events submitted by handlers, then emits the signal).
  A 1 s watchdog continues if the signal never comes. `advance` never re-enters:
  a second call waits for the first. The script runner waits for an `advance:`
  step to finish before its 60 ms gap starts.
- §4.4 `exportLog(path) → bool`: the write is an asynchronous XHR `PUT` (the
  synchronous one writes an empty file in Qt 6.8), and XHR reports status 0 for
  file URLs whether or not it worked. So `true` means "write started"; the result
  comes later through the `exported(path, ok)` signal (checked by reading the file
  back) and a warning on failure. The sim panel's "saved" label therefore means
  "started". The `export:` step waits for the write before continuing, so `--exit`
  never quits before the file exists.
- §3 `tracer.flashSet` stamps are `max(Date.now(), last + 1)`, so a repeat flash
  within the same millisecond still changes the value.
- `--grab`: QML cannot call `QQuickWindow::grabWindow`, and `grabToImage` on the
  window's content item fails ("item has no QML engine", it was created by C++).
  The host moves the content into a QML-created `Rectangle` in the window colour
  and grabs that, then quits. The image matches the C++ one.
- The machine is started by `StateMachineLoader` itself once the file is parsed
  (it has no "don't start" switch); all host objects are connected before that
  happens, and the script starts when `running` becomes true.
- `QScxmlStateMachine::log` reaches QML late (after `reachedStableState` and even
  after a 0 ms timer). The attribution of §4.3 is designed for that (D-18) and
  needed no change.
