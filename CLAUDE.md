# CLAUDE.md — conventions for this repo

- `docs/STATECHART-SPEC.md` is authoritative. Change the spec first, then
  `statechart/watch.scxml`, then the tests — in the same commit.
- All behaviour lives in `watch.scxml`. C++ (`src/`) only keeps values (clock,
  alarm times, stopwatch counter) and turns time into events; QML (`qml/`) only
  renders and forwards input. No mode logic in C++ or QML.
- Every `<transition>` carries a `<!-- T-xx -->` comment **and** a
  `<log label="T-xx" expr="_event.name"/>` (the log drives the live event log).
  `python3 tests/conformance.py` must pass (spec ↔ SCXML ↔ tests).
- Record every decision or deviation from the paper in `docs/DESIGN-NOTES.md`.
- Don't hand-edit generated files in `build/`.
- Cite Qt and SCXML doc URLs in comments for non-obvious APIs.
- Qt 6.4 is the minimum (distro packages); don't use APIs newer than that.
- Build/test: `cmake -B build -G Ninja && cmake --build build && ctest --test-dir build`
  (GUI parts run headless with `QT_QPA_PLATFORM=offscreen`).
