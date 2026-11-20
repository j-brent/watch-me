# watch-me — Rust host

The glue of [docs/HOST-CONTRACT.md](../docs/HOST-CONTRACT.md) written in Rust with
[cxx-qt](https://github.com/KDAB/cxx-qt) 0.10. It runs the shared statechart
(`shared/statechart/watch.scxml`) and the shared UI (`shared/qml/Main.qml`)
unchanged. The C++ host in `cpp/` is the reference this one was ported from.

## Setup

- Qt 6.8 with Qt SCXML, Qt Quick and Qt Quick Controls (e.g. `/opt/Qt/6.8.3/gcc_64`).
- Rust stable (built with 1.97) and a C++17 compiler (cxx-qt compiles C++).
- On Linux, the usual X11/EGL libraries for a Qt Quick window (see `.github/workflows/rust.yml`).

cxx-qt-build finds Qt through qmake: set `QMAKE`, or put `qmake6` on `PATH`.

## Build

```sh
cd rust
QMAKE=/opt/Qt/6.8.3/gcc_64/bin/qmake6 cargo build --release
```

Plain cargo, no CMake: `build.rs` runs cxx-qt-build, which generates the C++ for
the bridges, runs moc and compiles them together with the shim in `cpp/`. On
Linux it also sets an RPATH to Qt's `lib/`, so no `LD_LIBRARY_PATH` is needed.

## Run

```sh
rust/target/release/watch-me-rs                     # the window
rust/target/release/watch-me-rs --auto-insert --time 1987-06-01T10:08:30
QT_QPA_PLATFORM=offscreen rust/target/release/watch-me-rs --time 1987-06-01T10:08:30 \
    --script "battery.inserted;a;a;a;a;b" --grab /tmp/rs.png
```

The options are those of HOST-CONTRACT §5 (`--help` lists them). The shared files
(`shared/`, `docs/STATECHART-SPEC.md`) are read from disk at run time, from
`$WATCHME_ROOT` if set, else from the repository the executable sits in
(`rust/target/<profile>/` → three levels up), else from the checkout it was built in.

## Verify

```sh
python3 tools/verify.py --cmd "rust/target/release/watch-me-rs"   # 68/68 ok; uncovered: []
cd rust && QMAKE=… cargo test --release                            # date arithmetic
```

The exported event log of every one of the 68 walks is byte-identical to the C++
host's (compared with the same `--script …;export:` runs).

## Layout

| File | What |
|---|---|
| `cpp/shim.h`, `cpp/shim.cpp` | The C++ shim: `WatchScxml` + four window helpers |
| `src/ffi.rs` | Bridge to the shim and to `QTimer` |
| `src/qobjects.rs` | `WatchModel`, `TransitionTracer`, `ButtonRouter`, `ScxmlTreeModel`, `SpecTags`, `Beeper` as `#[qobject]`s |
| `src/host.rs` | The glue: event handling, clock, timeouts, buttons, event log |
| `src/watch.rs` | Watch values and wall-clock date arithmetic |
| `src/tree.rs`, `src/tags.rs` | SCXML tree (roxmltree), spec confidence tags |
| `src/main.rs` | Command line, start-up, QML window, `--script` runner |

## Notes for the comparison

**What the C++ shim does, and why.** cxx-qt wraps Qt Core/Gui/Qml types but not Qt
SCXML, and not `QQuickWindow`. `WatchScxml` (≈90 lines with the helpers) loads
`watch.scxml` with `QScxmlStateMachine::fromFile` (any `parseErrors()` is fatal)
and re-emits what the machine does as ordinary Qt signals with Qt types:
`connectToEvent(spec)` → `machineEvent(name, QVariantMap)`, `log` → `logged`,
`reachedStableState`/`runningChanged` → `stable`, `initializedChanged` →
`initialized`. Methods: `start`, `submit`, `submitWithData`, `activeStateNames`,
`isInitialized`, `hasDataModel`. Free functions: `drainMachine`
(`sendPostedEvents(MetaCall)`, D-21), `quitApp`, and `grabWindow`/`resizeWindow`/
`setChildProperty` for `--grab`, `--size` and `select:`. Rust connects to the
signals with cxx-qt's `extern "C++Qt"` + `#[qsignal]` (`on_machine_event(...)`).
There is no decision in C++: all glue logic is in `src/`.

**What was awkward in cxx-qt.**
- *Signals and closures.* `on_<signal>` closures must be `Send + 'static`, even
  though everything runs on the GUI thread. They cannot capture an `Rc` to shared
  state, so the host is a leaked, thread-local singleton reached through `host()`.
- *Re-entrancy and `&mut`.* The machine calls back into the host while the host is
  in the middle of something (`drainMachine` inside `TimeoutService.advance`,
  QML reading a property while a signal is emitted). A `#[qobject]`'s Rust state is
  reached through `Pin<&mut Self>`, so keeping state there would create a second
  `&mut` during the callback. The QObjects are therefore empty facades
  (`struct WatchModelRust;`) and all state is in `RefCell`s in `Host`; no borrow
  is held across a call that can re-enter. The C++ host has none of this to do.
- *QVariant / QVariantMap.* Usable: `QMap<QMapPair_QString_QVariant>` and
  `QList<QVariant>` nest inside `QVariant`, `FromIterator` builds them, and a
  `QObject*` goes into a `QVariant` via `QObjectMutPtr` for `setInitialProperties`.
  Reading is verbose (`data.get(&QString::from("id")).unwrap_or_default()
  .value_or_default::<QString>()`), and every key/string crosses as `QString`.
- *Bridges.* Each `#[qobject]` needs its own inner Rust type (one shared
  `Facade` type is rejected); every property with a custom getter needs the getter
  declared in the bridge; `#[allow]` is not accepted on the bridge module. A
  private Qt signal (`QTimer::timeout`) must be declared `pub(self)` or the
  generated C++ does not compile. Errors in the bridge show up as build-script
  failures with long C++ compiler logs.
- *Missing wrappers.* No `QTimer` in cxx-qt-lib (declared in `ffi.rs` with
  `make_unique`, no C++ needed), no `QCoreApplication::exit`, `QQuickWindow`,
  `findChild`, `QCommandLineParser` (that one is in cxx-qt-lib-extras; the
  options are parsed in Rust).
- *Build.* `cargo build` works without CMake once `QMAKE` is set; the first
  build compiles cxx-qt-lib's C++ (most of the time below). No RPATH is set on
  Linux by default (added in `build.rs`).

**Numbers** (this machine, 4 cores, Qt 6.8.3, release):
- Clean build: ~95 s (cargo, all dependencies) vs. ~18 s for the C++ `watch-me`
  target (Ninja, after configure). Incremental after editing one `.rs`: ~6 s.
- Binary: 1.8 MB stripped (`watch-me-rs`) vs. 1.2 MB for the C++ `watch-me`
  (which also embeds the compiled QML).
- Lines: Rust 2118 (`src/`, with comments, tests and the CLI help), C++ shim 142;
  the C++ host (`cpp/src/`) is 1530.

**Where this host differs from the contract or the C++ host.**
- The QML is loaded from `shared/qml/` on disk (`QUrl::fromLocalFile`), not
  compiled into a QML module; the SCXML is loaded at run time, not by
  `qt_add_statecharts`. So the executable needs the repository (or
  `WATCHME_ROOT`) at run time.
- `Beeper` is a stub: `available = false`, no sound (the contract allows it).
- The command line is parsed by hand: `--opt value` and `--opt=value`, `--help`;
  QCommandLineParser's other forms (e.g. `-h` bundling) are not reproduced. Qt's
  own `-platform x` / `-style x` are skipped.
- `--script` scheduling uses one `QTimer` re-armed for the earliest pending step;
  each step still runs in its own timer event, as with `QTimer::singleShot`.
