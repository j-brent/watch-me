# CLAUDE.md — conventions for this repo

- `docs/STATECHART-SPEC.md` is authoritative. Change the spec first, then
  `statechart/watch.scxml`, then the tests — in the same commit.
- All behaviour lives in `watch.scxml`. C++ (`src/`) only keeps values (clock,
  alarm times, stopwatch counter) and turns time into events; QML (`qml/`) only
  renders and forwards input. No mode logic in C++ or QML.
- Every `<transition>` carries a `<!-- T-xx -->` comment **and** a
  `<log label="T-xx" expr="_event.name"/>` (the log drives the live event log).
  The one exception is the default transition inside a `<history>`: it is not a
  spec row (STATECHART-SPEC §3, DESIGN-NOTES D-22).
  `python3 tests/conformance.py` must pass (spec ↔ SCXML ↔ tests): each row is
  asserted with `FIRED(w, "T-xx")`, or `ROW_CHECK("T-xx", …)` for a row with no
  `<log>` (T-15, T-56); a mention in a comment does not count.
- Behaviour changes that alter the walk also need `tools/verify.py` re-run and
  `docs/VERIFICATION.md` committed: the ctest `verify` fails when it is stale.
- Timeouts ("n sec in X") are `timer.start`/`timer.cancel` sends answered by
  `TimeoutService` with a generation-guarded `timeout.<id>` (DESIGN-NOTES D-20).
  Tests move time with `advance()`; never sleep or assert on wall-clock time.
- Record every decision or deviation from the paper in `docs/DESIGN-NOTES.md`.
- Don't hand-edit generated files in `build/`.
- Cite Qt and SCXML doc URLs in comments for non-obvious APIs.
- Qt 6.8 LTS is the requirement (`find_package(Qt6 6.8 …)`); CI builds with it on
  Ubuntu, Windows and macOS.
- Build/test: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
  (GUI parts run headless with `QT_QPA_PLATFORM=offscreen`).
