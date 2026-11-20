# CLAUDE.md — conventions for this repo

- `docs/STATECHART-SPEC.md` is authoritative. Change the spec first, then
  `shared/statechart/watch.scxml`, then the tests — in the same commit.
- All behaviour lives in `shared/statechart/watch.scxml`. Hosts (`cpp/src/`, and
  `python/`, `rust/`, `qmljs/`, `qmlts/` as they are added) only keep values (clock,
  alarm times, stopwatch counter) and turn time into events; QML (`shared/qml/`)
  only renders and forwards input. No mode logic in a host or in QML.
- Hosts talk to the statechart through events only, never the datamodel
  (DESIGN-NOTES D-24). `docs/HOST-CONTRACT.md` is the contract every host follows;
  change it together with `cpp/` when the glue changes. Every host must pass
  `python3 tools/verify.py --cmd "<host command>"`.
- Every `<transition>` carries a `<!-- T-xx -->` comment **and** a
  `<log label="T-xx" expr="_event.name"/>` (the log drives the live event log).
  The exceptions are the default transition inside a `<history>` (not a spec row,
  STATECHART-SPEC §3, DESIGN-NOTES D-22) and the `host.*` boundary transitions (D-24).
  `python3 cpp/tests/conformance.py` must pass (spec ↔ SCXML ↔ tests): each row is
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
