# Host comparison: C++, Python, Rust, QML/JS, QML/TS

Five implementations of the same glue (`docs/HOST-CONTRACT.md`) around one
statechart (`shared/statechart/watch.scxml`) and one UI (`shared/qml/`), all on
Qt 6.8.3. Every host passes the same 68-row walk (`tools/verify.py --cmd …`), and
its exported event logs are byte-for-byte identical to the C++ host's in all 68
runs. So the comparison below is between implementations that behave the same,
not approximately the same.

| Host | Directory | Pull request |
|---|---|---|
| C++ (reference) | `cpp/` | #1, #2 |
| Python (PySide6 6.8.3) | `python/` | #4 |
| Rust (cxx-qt 0.10 + C++ shim) | `rust/` | #6 |
| QML/JavaScript (Qt's `qml` tool, nothing compiled) | `qmljs/` | #3 |
| QML/TypeScript (`tsc` → `.mjs`, Qt's `qml` tool) | `qmlts/` | #5 |

Measured in one Linux container (4 cores). Absolute times will differ on other
machines; the ratios are what matter.

## 1. Size of the glue

Lines including comments and blank lines; the shared QML and SCXML are not counted.

| Host | Glue | Notes |
|---|---|---|
| C++ | 1,530 | headers included |
| Python | **1,097** | the smallest |
| QML/JS | 1,302 | QML components + 2 `.mjs` helpers + a 40-line launcher |
| QML/TS | 2,140 | 1,668 TypeScript + 426 QML wrappers + 46-line launcher |
| Rust | 2,309 | 2,118 Rust + 142 C++ shim + 49 build files |

## 2. Development

| | C++ | Python | Rust | QML/JS | QML/TS |
|---|---|---|---|---|---|
| Toolchain | CMake, Ninja, C++20 compiler, Qt SDK | Python 3, `pip install PySide6==6.8.3` | Rust, C++17 compiler, Qt SDK, `QMAKE` set | Qt SDK only | Qt SDK, Node, `npm ci` |
| Build step | yes (~18 s for the app) | none | yes (~95 s clean, ~6 s incremental) | none | `npm run build` (seconds) |
| Edit → run | rebuild | run | rebuild | run | rebuild TS, run |
| Dependencies on disk | Qt SDK | 658 MB venv (PySide6 bundles its own Qt) | Qt SDK + cargo registry | Qt SDK | Qt SDK + 23 MB `node_modules` |
| SCXML | compiled (`qt_add_statecharts`) | loaded at run time (`fromFile`) | loaded at run time (shim) | `StateMachineLoader` | `StateMachineLoader` |
| Follows the contract exactly | yes (reference) | yes | yes | 3 documented deviations | same 3 deviations |

## 3. Running

The same start-up measured 5 times per host (median). "core" is `--no-gui` with
one script step, then exit; "gui" also creates the window and grabs a screenshot.
Both include the contract's fixed 200 ms delay before the first script step.

| Host | core | gui | Peak memory (core / gui) | Binary |
|---|---|---|---|---|
| C++ | 280 ms | 448 ms | 32 / 80 MB | 2.0 MB (1.2 MB stripped) |
| Rust | 276 ms | 450 ms | 30 / 78 MB | 1.8 MB stripped |
| QML/JS | 300 ms | 513 ms | 33 / 85 MB | — (`qml` tool) |
| QML/TS | 312 ms | 547 ms | 40 / 86 MB | — (`qml` tool) |
| Python | 390 ms | 582 ms | 49 / 126 MB | — (interpreter) |

The 68-row walk takes 27–30 s for every host. It is dominated by the script's own
waits, so it says nothing about speed. At this application's event rates (a few
events per button press), every host is fast enough. The differences are start-up
and memory, and Python's extra ~50 MB in the GUI case is the interpreter plus PySide6's
binding layer.

## 4. What each binding made hard

**Python (PySide6).** The closest to C++. `sendPostedEvents` works, so
`TimeoutService.advance` is synchronous as the contract says.
- `connectToEvent` exists only in the `SLOT("…")` string form, so each callback
  is a small QObject with a `@Slot(QScxmlEvent)`.
- An event without data has `data == ""`, not `{}`.
- `rootObjects()[0]` is a plain `QWindow` unless `PySide6.QtQuick` is imported.
- Exceptions in `QTimer.singleShot` callbacks are printed and swallowed, so a
  failing script step stalled the script until the runner caught them itself.

**Rust (cxx-qt).** It works and is as fast and small as C++, but it took the
most code and the most workarounds.
- cxx-qt has no Qt SCXML, `QQuickWindow`, `QTimer` (declared by hand) or
  `QCoreApplication::exit`, hence the 142-line C++ shim.
- Signal closures must be `Send + 'static`, so the host state is a thread-local singleton.
- The state machine calls back into the host in the middle of a call. Keeping
  state in a `#[qobject]` would create a second `&mut`, so the QObjects are empty
  facades and the state lives in `RefCell`s.
- Mistakes in the bridge show up as long C++ build errors. `QVariantMap` access is verbose.

**QML/JS.** No compiled code at all, and the glue is small. But QML's sandbox
is visible everywhere:
- No datamodel access. This is why the contract became event-only (D-24).
- No synchronous event draining, so `TimeoutService.advance` is asynchronous (deviation 1).
- File I/O only through `XMLHttpRequest`, with `QML_XHR_ALLOW_FILE_READ/WRITE`
  set in the environment. A file `PUT` cannot report failure in Qt 6.8, so
  `exportLog` returns "started", and the result is checked by reading the file back (deviation 2).
- No working directory, so the launcher passes `--cwd` (deviation 3).
- No `connectToEvent` (`EventConnection` instead), `findChild`, `grabWindow`, or
  logging-rule API. `responseXML` drops comments, so it has its own XML tokenizer.
- No audio: `Beeper.available` is false.

**QML/TS.** Everything above applies, plus a build step.
- Qt's JS engine (V4) rejects `async`/`await`, object spread, class fields and
  `#private`, so the target is ES2016. A `.mjs` module has no `Qt`, timers or
  file API, so QML passes them in through typed "ports".
- The type checker found no bug that a test run would not have found. What it
  bought was guard rails: an exhaustive union of script steps, forced handling of
  missing values, and branded time types.
- Every surprise was at the hand-written boundary to Qt, which the types cannot check.

## 5. What this means for a larger C++ application

1. **The boundary decides the cost, not the language.** With all behaviour in
   SCXML and an event-only boundary (D-24), four very different languages produced
   identical behaviour from the same spec in one pass. Make the C++ → SCXML/QML
   boundary events-only first. That is the step that makes the rest portable.
2. **A conformance walk that drives the real app through its CLI is what makes a
   port safe.** Every host was accepted on `tools/verify.py` alone. Unit tests in
   each language were not needed to trust the result.
3. **Python (PySide6) is the cheapest second language.** It has the least code and
   no build. Its APIs map almost 1:1 to C++. The costs are ~100 ms more start-up,
   ~50 MB more memory and a large runtime to ship. It is a good fit for glue,
   tooling and business rules, and less so for hot paths.
4. **QML/JS can carry glue, but not core logic.** It has no file system, no
   synchronous control of the event loop, no datamodel and no audio. It is fine
   for view logic and thin adapters. Keep the core out of it.
5. **TypeScript on QML adds a build step for modest gain**, because the risk sits
   at the untyped Qt boundary. It is worth it only if a team already lives in TypeScript.
6. **Rust matches C++ at run time but costs the most to write against Qt.** It
   needs C++ shims for every unwrapped Qt module, and its ownership model fights
   Qt's re-entrant callbacks. It is a better fit for a core library behind a C
   ABI or for a separate process than for QObject glue.
