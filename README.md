# watch-me

An executable version of the most famous statechart in software engineering: the
**Citizen Quartz Multi-Alarm III** digital watch from David Harel's 1987 paper
*"Statecharts: A Visual Formalism for Complex Systems"*. It is built with Qt Quick
and Qt SCXML.

Harel's statecharts are the basis of UML state diagrams and of SCXML. Qt's
State Machine framework also cites the paper as its source of concepts and notation.
This project closes the loop: it takes the paper's full watch statechart (Fig. 31),
writes it as one SCXML document, and runs it behind an interactive watch with a
live view of the statechart.

> **Status: planned, not yet implemented.** This repo currently contains the
> implementation plan and the behaviour spec. The code is to be written by
> following `PLAN.md`, phase by phase.

## What it will do

- **Watch face.** A seven-segment LCD with four buttons (a, b, c, d), backlight,
  beeper, alarm and chime indicators, and a low-battery blink. Mouse press and
  release, or holding keys A/B/C/D, send the button events.
- **The whole statechart, in SCXML.** All six orthogonal components of `alive`
  are modelled: main displays and alarm beeping, alarm 1, alarm 2, chime, light
  and power. The model also covers time/date update, alarm setting, the
  stopwatch, beep-test, deep history and timeouts. All behaviour lives in
  `statechart/watch.scxml`. C++ holds only values (clock, alarm times,
  stopwatch counter), and QML only renders.
- **Live statechart view.** The SCXML is parsed at runtime and drawn as
  literally nested states, with dashed dividers between orthogonal regions as
  in Harel's figures. The active configuration is highlighted, and transitions
  flash and are logged as they fire. A legend gives each visual channel a
  single meaning.
- **Simulation panel.** Clock speed, set the time, jump to just before an alarm,
  battery weakens/dies/removed/inserted, and the Fig. 32 quirk on or off.

## Repository contents

| Path | Purpose |
|---|---|
| `PLAN.md` | Implementation plan: architecture, repo layout, SCXML conventions, phases P1–P8 with acceptance criteria, risks |
| `docs/STATECHART-SPEC.md` | **Source of truth** for behaviour. Covers events, the state hierarchy, and every transition as a numbered row (`T-xx`), each tagged by confidence |
| `docs/harel1987.pdf` | The paper (44 pp., from https://www.state-machine.com/doc/Harel87.pdf), used to verify the spec (Step 0 of the plan). Kept for private study; don't make the repo public with it included |

### Spec confidence tags

The spec was drafted without a legible copy of Fig. 31, so every row says where
it came from:

- **[P]** stated in the paper's prose
- **[F]** reconstructed from the figures; **must be checked against Fig. 31**
- **[A]** an assumption needed to make the model executable

Step 0 of `PLAN.md` resolves all [F] and [A] rows against the paper before any
code is written. Corrections are recorded in `docs/DESIGN-NOTES.md`.

## Building (once implemented)

Requirements: Qt ≥ 6.5 with the Qt Quick, Qt Quick Shapes, Qt SCXML and Qt Test
modules; CMake ≥ 3.21; Ninja; a C++20 compiler; Python 3 for the conformance
check.

```sh
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/<platform>
cmake --build build
ctest --test-dir build
./build/watch-me
```

If no Qt installation is available, one way to get one is `aqtinstall`:

```sh
pip install aqtinstall
aqt install-qt linux desktop 6.8.0 linux_gcc_64 -m qtscxml qtshapes   # adjust host/version
```

## Working on this repo with Claude Code

1. Start a Claude Code session on this repo with:
   > Read PLAN.md and docs/STATECHART-SPEC.md and implement the plan phase by
   > phase, starting with Step 0. Commit after each phase.
2. The plan has Claude Code create `CLAUDE.md` with the repo conventions in
   phase P1. The main rule: change the spec first, then the SCXML, then the
   tests, and keep every transition traceable to a `T-xx` row.

## References

- D. Harel, "Statecharts: A Visual Formalism for Complex Systems",
  *Science of Computer Programming* 8 (1987) 231–274. Copy hosted by the
  University of Edinburgh:
  https://www.inf.ed.ac.uk/teaching/courses/seoc/2006_2007/resources/statecharts.pdf
- Qt State Machine framework overview (cites Harel as the basis of its concepts and notation):
  https://doc.qt.io/qt-5/statemachine-api.html
- Qt SCXML: https://doc.qt.io/qt-6/qtscxml-index.html
- W3C State Chart XML (SCXML) Recommendation: https://www.w3.org/TR/scxml/
