# Design notes

Decisions, deviations and mappings. Read together with `STATECHART-SPEC.md`
(behaviour) and `PLAN.md` (construction).

## 1. Step 0 — spec verified against Harel 1987

**Method.** `docs/harel1987.pdf` is a scanned PDF with no text layer. All 44 pages
were rendered with `pdftoppm -r 200` (Fig. 31 again at 400 dpi for close-ups) and
the prose was OCR'd with `tesseract` to cross-check the reading. PDF page *n* is
journal page 230 + *n*. Figures used:

| Figure | Journal page (PDF page) | Content |
|---|---|---|
| Fig. 8 | 237 (7) | displays ↔ alarms-beep, P1/P2/P |
| Fig. 9 | 237 (7) | display cycle on `a`, time/date on `d`, 2 min in date |
| Fig. 10–12 | 239 (9) | history; alarm1 = H → off/on toggled by d |
| Fig. 13 | 240 (10) | displays with H\*, wait/update, alarm/chime off/on |
| Fig. 14 | 240 (10) | update fields sec→…→mode, mode -c→ time, d → H |
| Fig. 15 | 241 (11) | update1 hr→10min→1min, 1min -c→ alarm1, d → H |
| Fig. 25 | 246 (16) | stopwatch, H\*, zero / (display ∥ run) |
| Fig. 26–28 | 246–248 (16–18) | dead/alive, six components, status regions |
| Fig. 29–30 | 249 (19) | beep-test as 00/10/01/beep component |
| Fig. 31 | fold-out (21–22) | **full watch** |
| Fig. 32 | 250 (20) | beep refined into on/off on `a`/`â` |

Every figure was legible at 200–400 dpi; nothing had to be left unverified.

**Changes** (row → old → new → figure). Rows whose content was confirmed
unchanged were simply retagged [F]/[A] → [P]; they are listed at the end.

| Row | Old | New | Figure |
|---|---|---|---|
| §2 hierarchy: `regular` | XOR group {time, date, wait, update, beep-test} | AND-state {(timedate: time, date, update) ∥ beep-test}; **wait is outside regular** | Fig. 31, prose p. 249 |
| §2 `beep-test` | a display state entered when b and d are held | orthogonal component of `regular` with states 00 (default), 10, 01, beep | Fig. 29, 31 |
| §2 `update.mode` | [A] "verify; drop if not in Fig. 31" | exists, last field | Fig. 14, 31; p. 236 |
| §2 `alarm1`/`alarm2` | `show` (default) + `update1` substates | alarm1 = H → **off \| on** (display of status); `update1` is a **sibling** of alarm1 inside `out` | Fig. 13, 15, 31 |
| §2 `chime` | atomic | H → off \| on | Fig. 13, 31 |
| §2 `stopwatch` entry | shallow history H [A] | **deep history H\*** | Fig. 25, 31 |
| §2 `out` | [F] wrapper | confirmed: contains alarm1, update1, alarm2, update2, chime | Fig. 31 |
| §2 `chime-status.enabled` | [F] quiet/beep | confirmed | Fig. 28, 31 |
| §1 `T.hits.hour` | [F] | [P], "T is whole hour" | Fig. 28, 31 |
| §1 `tick.second` | [A] event | removed: no transition reacts to it; display refresh is a C++ property | — |
| T-02 | alive → dead on battery.dies | **power.blink** → dead on battery.dies, action clh(main\*) | Fig. 26 ("weak batt. dies"), 28, 31 |
| T-03 | alive → dead | + action clh(main\*) | Fig. 28, 31 |
| T-10..T-14 | [F] | [P]; T-10/11/12 enter through the alarm/chime H; T-14 targets `regular` | Fig. 31 |
| T-15 | [A] date a.down: assume nothing | [P] no `a` transition from date, update, update1/2, wait | Fig. 31 |
| T-20 | time d.down → date | + condition **"b up"** (`In('bt_00') \|\| In('bt_01')`) | Fig. 31 ("d(b up)") |
| T-22 | 120 s in date | 2 min in date, timer restarted by any button (see D-3) | Fig. 9, 31; p. 247 |
| T-25 | → update (→ sec) | → update.sec explicitly | Fig. 14, 31 |
| T-26 | sec→1min→10min→hr→mon→date→day→year→**mode→sec**, [F]/[A] | sec→1min→10min→hr→mon→date→day→year→mode; **mode -c→ time** (exits update) | Fig. 14, 31 |
| T-27 | internal self-transition | unchanged behaviour; noted that the paper draws d → H (see D-4) | Fig. 14 |
| T-29 | [A] 120 s with no button | [P] "2 min in update" → time | Fig. 31, p. 247 |
| T-30 | alarm1.show → update1.hr | alarm1 (from off or on) → update1.hr | Fig. 15, 31 |
| T-32 | update1.1min → alarm1.show | update1.1min → alarm1 (via H, i.e. back to off/on as left) | Fig. 15, 31 |
| T-33 | [F] | [P] (d → H in update1) | Fig. 15, 31 |
| T-34 | update1 → alarm1.show | update1 → alarm1 (via H) | Fig. 15, 31 |
| T-35..T-39 | one row "same with 2" | split into five explicit rows | Fig. 31 |
| T-40 | out → time, 120 s no button | out → **regular** (default → time), "2 min in out (no change)" | Fig. 31 |
| T-41 | toggle on d.down [In('alarm1_show')] | disabled→enabled on d [in alarm1.off]; enabled→disabled on d [in alarm1.on] | Fig. 28, 31 |
| T-42 | [In('alarm2_show')] | [in alarm2.off] / [in alarm2.on] | Fig. 28, 31 |
| T-43 | [In('chime')] | [in chime.off] → enabled.quiet / [in chime.on] → disabled | Fig. 28, 31 |
| T-44..T-46 | — | **new**: alarm1/alarm2/chime display off ↔ on on d | Fig. 10, 13, 31 |
| T-47 | (folded into note under §3.4) | **new row**: restart of the 2-min timers on any button | p. 247 |
| T-55 | nonzero → zero, d [In(run_off) && In(disp_reg)] | **reg** → zero, d [in off] (same behaviour, source is reg) | Fig. 25, 31 |
| T-56 | short beep on entering/leaving run.on/off | half-second beep on T-50/T-51/T-52 | p. 258 |
| §3.5 history | "Re-entry uses shallow history [A]" | deep history H\* [P] | Fig. 25, 31 |
| T-63, T-64 | → displays (H\*) | unchanged, confirmed (arrows end at the H\* of displays) | Fig. 13, 31 |
| T-70, T-71 | [F] | [P]; T-71 "2 sec in beep" | Fig. 28, 31 |
| T-73 | light.on b.up → off | unchanged, confirmed (b̂) | Fig. 28, 31 |
| T-74 | regular.{time,date,update.\*} d.down [bHeld] or b.down [dHeld] → beep-test | **beep-test.00 -b→ 10** | Fig. 29, 31 |
| T-75 | beep-test b.up or d.up → regular (H) | **beep-test.00 -d→ 01** | Fig. 29, 31 |
| T-76 | "not reachable from wait, …" | **beep-test.10 -d→ beep** (scope is now structural, see §3.7 note) | Fig. 29, 31 |
| T-77..T-81 | — | **new**: 01 -b→ beep, 10 -b̂→ 00, 01 -d̂→ 00, beep -d̂→ 10, beep -b̂→ 01 | Fig. 29, 31 |
| T-82, T-83 | Fig. 32 as "beep-test.muted" substate | beep = on (default) -a→ off -â→ on, guarded by `harelAnomalies` | Fig. 32 |
| §1 events | release written as overbar | Harel draws release as a hat (â, b̂, ĉ, d̂) | Fig. 13, 29 |
| Initial state | dead [A] | dead [P] | Fig. 26, 28, 31 |

Retagged [F]/[A] → [P] with no change in content: T-01 source tag, T-21, T-23,
T-24, T-28, T-31, T-50..T-54, T-60..T-64, T-72.

## 2. Decisions (D-n)

- **D-1 Qt version.** The project requires **Qt 6.8 LTS** (`find_package(Qt6 6.8)`);
  CI builds and tests with Qt 6.8.3 on Ubuntu, Windows and macOS. History: the
  first development environment could not reach download.qt.io, so the code was
  written and tested against Ubuntu 24.04's Qt 6.4.2 packages, and for a while Qt
  6.4 was the documented minimum with its own CI job. That was an artifact of the
  environment, not a requirement, and was dropped. The Qt 6.4 workarounds (D-2,
  D-11, D-12, D-18) remain: they are correct on 6.8 as well.
- **D-2 clh(main\*).** Emulated with the `hv` ("history valid") datamodel object,
  see spec §3.1. Only entries whose history could predate the last `dead` need the
  guard: T-10 (alarm1), T-11 (alarm2), T-12 (chime), T-13 (stopwatch). T-32/T-34
  (back from update1), T-37/T-39 and T-63/T-64 (back to displays) always follow an
  exit of the same state in the same life, so they target the H directly.
- **D-3 2-minute returns.** Fig. 31 labels them "2 min in date", "2 min in update",
  "2 min in out (no change)". The prose (p. 247) describes one feature: "a 2-minute
  automatic return to time from all displays other than the stopwatch, on condition
  that no button has been depressed in the interim". All three are therefore
  implemented as inactivity timers restarted by any button (T-47). In `date` only
  a/b/c could restart it (d leaves date), so the difference to a literal
  `timeout(entered date, 120)` is only visible if you press a, b or c in date.
- **D-4 d → H inside update/update1/update2.** Harel draws `d` as exit + re-entry
  through H ("causes exit (but not exit from the encapsulating superstate) and
  immediate entrance to the most recently visited substate"). In SCXML the history
  of `update` is only recorded when `update` itself is exited, so a d → H
  transition would restore a stale field. The rows are implemented as targetless
  transitions in `update` that send `model.increment` with the field derived from
  `In()`. The configuration after the step is identical.
- **D-5 "b up" condition (T-20).** In Fig. 31 the beep-test component tracks b and
  d (b is down in 10 and beep). The condition is expressed as
  `In('bt_00') || In('bt_01')` instead of a separate held-button flag, so there are
  **no `bHeld`/`dHeld` datamodel flags** (this deviates from PLAN §5, which
  predates Step 0). Consequence, same as in the paper: if b is already held when
  `regular` is entered, beep-test starts in 00 (Harel: "assuming, for simplicity,
  that b and d cannot be depressed simultaneously" — p. 249).
- **D-6 Stopwatch counter.** Driven by outgoing events on transitions
  (`sw.start` on T-50/T-52, `sw.stop` on T-51, `sw.reset` on T-55, `sw.lap` on T-53)
  rather than by `run.on` being active, because `run.on` is not active while
  another display is shown, but the stopwatch keeps counting. The counter keeps
  simulated milliseconds and only shows centiseconds, so stopping does not round a
  run segment down (each stop used to lose up to 9 ms).
- **D-7 alarms-beep display.** The paper notes "the displays do not change while
  the alarm is beeping" but defers it to activities. The LCD shows the time display
  with the whole LCD flashing.
- **D-8 Battery insertion.** On `alive` entry the C++ model resets alarms (T1 = T2 =
  12:00), the stopwatch and 12/24h mode to defaults, and seeds the clock from the
  host clock. [A] The paper is silent.
- **D-9 Timeouts and simulation speed.** The statechart's timeouts run on their own
  clock (`TimeoutService`, D-20) at 1× real time in the app. The sim panel's
  clock-speed slider affects only the watch's time T (alarms, chime), not the
  2-second / 30-second / 2-minute interaction timeouts. *(Superseded detail: the
  first version used SCXML `<send delay>` scaled by a `timeScale` datamodel value.)*
- **D-10 "Beep-test from date" scenario (PLAN P3).** The plan expected
  "beep-test from date, release d → date". In Fig. 31 `d` in `date` goes to `time`
  unconditionally (T-21), so pressing b then d in date ends in time with the beeper
  on, and releasing d keeps time. Pressing d first in *time* (→ date, beep-test 01)
  and then b keeps the watch in date with the beeper on. The test checks the
  paper's behaviour for both orders.
- **D-11 Outgoing events.** PLAN §3 proposed `<send type="qt:signal">`. Qt 6.4's
  SCXML runtime rejects that type ("qt:signal is not a valid type",
  error.execution). Outgoing events are therefore plain `<send event="…">`s to
  the machine's own external queue; `QScxmlStateMachine::connectToEvent` observes
  them, including their `<param>` data. They are processed afterwards as no-op
  events (no transition listens to `model.*`, `sw.*`, `beep.*`). They are
  asynchronous: C++ sees them on the next event-loop turn, in order. Outgoing
  events: `model.reset` (entry of alive), `model.increment {field}` (T-27, T-33,
  T-38), `sw.start` (T-50, T-52), `sw.stop` (T-51), `sw.lap` (T-53),
  `sw.reset` (T-55), `beep.short` (T-56).
- **D-12 Transition priority in Qt 6.4.** With the beeper on in `time`, `a.down`
  enables T-10 (time → alarm1, leaves regular) and T-82 (beep.on → off). Both the
  W3C algorithm (document order) and Harel/Statemate (outer scope first) choose
  T-10; Qt 6.4 chose T-82. T-82's condition carries `!In('time')`, so the model
  behaves as specified on any conforming engine. No other button event enables two
  conflicting transitions (checked: all other simultaneous transitions are in
  orthogonal components or targetless).
- **D-13 SCXML errors are test failures.** `tst_watch` installs a message
  handler on `qt.scxml.statemachine` and fails any test during which the engine
  reported `error.execution` (Qt reports those only as debug output).
- **D-14 Button placement.** PLAN P5 assumed a/b left and c/d right [A]. Fig. 7 of
  the paper (journal p. 236) shows **a** at the upper left and **b, c, d** down the
  right side (top, middle, bottom). That is now the default in
  `statechart/layout.json` (`buttons`). The LCD layout loosely follows Fig. 7: a
  row of four indicator windows (chime, alarm 1, alarm 2, stopwatch running) above
  the main digits, with the AM/PM/24H indicator next to the seconds. No
  manufacturer branding is reproduced.
- **D-15 LCD content.** Spec §4 is implemented in `qml/Lcd.qml` as a pure
  function of the active configuration and the WatchModel values. The `day`
  update field changes the weekday independently of the calendar date (a weekday
  offset), as on simple quartz watches. `update.year` shows the 4-digit year. The
  stopwatch shows `MM:SS cc` and wraps after 60 minutes.
- **D-16 Audio.** Optional and off by default: `Beeper` synthesises a 0.5 s tone
  (T-56) and a looping two-tone beep (alarms-beep, chime.beep, beep-test beep.on)
  with `QSoundEffect` when Qt Multimedia is available at build time. The visual
  "BEEP" indicator on the LCD is always shown.
- **D-17 Statechart view.** Generated at run time from the embedded
  `watch.scxml` (`ScxmlTreeModel`). `layout.json` only gives `rows`/`columns` per
  state; region order is the document order. Labels drop the parent prefix
  (`update_sec` is shown as `sec` inside `update`); hovering or clicking shows the
  full transitions with their T-xx. The unnamed AND-state inside `stopwatch` is
  `sw_nonzero` (drawn as "nonzero"), and the unnamed XOR component of `regular`
  is `timedate`. The default arrow (●→) on `out`'s first child comes from SCXML
  needing an initial child; the paper never enters `out` by default.
- **D-18 Event-log attribution.** Qt 6.4 emits `QScxmlStateMachine::log` *after*
  the macrostep (after `reachedStableState`). `TransitionTracer` therefore
  resolves each `T-xx` log to a transition whose source was active in one of the
  last four stable configurations and whose target is active now. For the
  clh(main\*) pairs it reads the `hv_*` flag to tell the history branch from the
  default branch. This is exact for interactive use (one button event per
  macrostep). A burst of many queued events can mis-attribute the source/target
  text of a line; the T-xx itself is always exact.
- **D-19 CI and releases.** GitHub-hosted runners can reach download.qt.io, so CI
  uses Qt 6.8 LTS (via `jurplel/install-qt-action`) on Ubuntu, Windows and macOS.
  Packaging uses Qt's own deployment (`qt_generate_deploy_qml_app_script`,
  behind `-DWATCHME_DEPLOY=ON`), so `cmake --install` produces a
  self-contained tree on all three platforms. The release workflow reuses the CI
  workflow (`workflow_call`), so published packages are exactly the tested
  ones. To make the code build on MSVC: `/utf-8` (non-ASCII literals) and
  `std::numbers::pi` instead of `M_PI`.
- **D-20 Timeouts on an explicit clock.** The first version implemented
  Harel's `timeout(entered X, n)` with SCXML delayed `<send>`s on Qt's real timers,
  and tests shrank them with `timeScale` (2 min → 120 ms) and slept. Those tests
  measured the runner's scheduling as much as the watch: on Windows CI a 2 ms
  "2 sec in wait" expired between two button events and failed a correct build. A
  failure could not be told apart from a real bug. Now:
  - `watch.scxml` still decides every timer: entering X sends
    `timer.start{id, secs, gen}`, leaving X sends `timer.cancel{id}`, and T-47
    re-sends `timer.start` on a button press. Which timers exist, how long they
    run, and when they restart or cancel are all in the statechart (and visible in
    the view).
  - `TimeoutService` (C++) only counts time and submits `timeout.<id>{gen}` when a
    deadline passes. The app advances it from a real-time QTimer at 1×.
  - Because the outgoing events are asynchronous (D-11), a timeout can already be
    queued when X is left. Each timer carries a generation number (`tg_<id>`,
    incremented on every start) and the timeout transition is guarded by
    `_event.data.gen == tg_<id>`, so a timeout from an earlier visit is ignored.
  - Tests advance the clock explicitly and check the exact boundary (at
    119.999 s still in `date`, at 120 s in `time`). There are no sleeps or
    wall-clock assertions, `tst_watch` runs in ~0.2 s instead of ~9 s, and
    deliberately breaking a duration, the restart, the cancel or the generation
    guard each makes a test fail.
  - `tools/verify.py` runs the app with `--manual-time` and moves time with the
    `advance:ms` / `clock:ms` script steps.
- **D-21 Timeouts handled at their own deadline.** `TimeoutService::advance(ms)`
  used to submit every deadline in the step and only then let the machine run, so
  a timer started *by* a timeout (wait's 2 s leading to update's 2 min) was armed
  at the end of the step: `advance(122 s)` from `wait` stayed in `update`. Now,
  after submitting each `timeout.<id>`, the service lets the machine process it
  (`QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall)`: Qt SCXML runs
  its queue from a queued meta-call) with the clock at that deadline, so the
  timers it starts count from there and can fire within the same step. It also
  drains once before the step, so timers requested by input submitted just before
  start at the current time. `advance` does not re-enter itself.
  Observed in Qt 6.8: `connectToEvent` handlers for the machine's own sends run
  while the send executes, inside the macrostep, so `model.reset` (sent on entry
  to `alive`) reaches `WatchModel` before any button already queued behind
  `battery.inserted` (checked by `tst_model::resetBeforeQueuedInput`).
- **D-22 History default transitions carry no T-xx.** The default transition
  inside a `<history>` (`displays_H`, `alarm1_H`, `alarm2_H`, `chime_H`, `sw_H`) says
  where the H goes before it has a value; it is part of the H connector, not a
  transition of the paper's figure, and no event takes it. It has no spec row,
  `<!-- T-xx -->` or `<log>`; `conformance.py` and `ScxmlTreeModel` skip it, and
  CLAUDE.md states the exception.
- **D-23 Test strategy after the test review.** A review against Myers' method
  (with mutation probes as a cross-check) found the suites strong on transitions
  and timer boundaries but thin on compound conditions, history after a battery
  cycle, "any button" rows, unexpected input and the code around the statechart.
  Changes:
  - `tst_watch`: P2 with T1 == T2; history recorded as "on" and forgotten after
    `battery.removed`; T-47 and T-63 with every button (data-driven); events that
    must change nothing (dead, `battery.inserted` while alive, a second
    `battery.weakens`, releases without presses).
  - `tst_model`: setter clamping and wrapping (now stated in `WatchModel.h`),
    `jumpBeforeAlarm` rolling to tomorrow, alarm and hour at midnight. Every
    event is settled before the next step, except where a test queues on purpose.
  - `tst_input` (new): `ButtonRouter` with repeated, unheld and invalid input,
    `releaseAll`, and the datamodel values behind the sim panel.
  - `conformance.py` counts assertions, not mentions: `FIRED(w, "T-xx")` for rows
    with a `<log>`, `ROW_CHECK("T-xx", …)` for the two rows without one; comments
    are stripped first. `fired` accumulates until `reset()`, so tests reset before
    the step a FIRED is meant to check.
  - `tools/verify.py` runs as the ctest `verify` on every CI platform, with
    `--check`: it fails on any row and when `docs/VERIFICATION.md` is stale. The
    `export:` script step takes the rest of the step as the path, so Windows paths
    (`C:\…`) work. The walk runs the app with `QT_QPA_PLATFORM=offscreen` and
    `QT_QUICK_CONTROLS_STYLE=Fusion`. On macOS CI every run crashed at start-up
    (SIGSEGV) under offscreen; the default controls style there is the native
    "macOS" style, which draws through AppKit and needs the cocoa platform (the
    deployed app's smoke test, which runs on cocoa, passes). A first attempt with
    Qt Quick's software renderer did not change the crash and was reverted.
  - The LCD content moved from `Lcd.qml` to `qml/LcdView.mjs`, a pure function of
    the configuration and the watch values, tested by `tst_lcd` (QJSEngine, no
    window). Spec §4 now fixes the two open points: 24-hour hours are zero-padded
    (`00:00` at midnight; it used to show ` 0:00`), and the stopwatch display wraps
    from `59'59"99` to `00'00"00` while it keeps counting (it already did; now it
    is specified and tested).

## 3. Open deviations from the paper (summary)

None of these changes behaviour that the paper specifies; each is listed with the
decision that explains it.

1. `clh(main*)` is emulated with `hv_*` flags and guarded history entries (D-2).
2. `d → H` in update/update1/update2 is implemented as targetless increments. The
   view therefore shows no H connector in those states (D-4).
3. All three 2-minute returns restart on any button: the prose describes the
   feature that way, while the figure labels are "2 min in date/update" (D-3).
4. "b up" in T-20 is expressed through the beep-test states (D-5).
5. T-82 carries `!In('time')`, so T-10 wins on Qt 6.4 as the W3C and Statemate
   semantics both require (D-12).
6. Fig. 32 is included behind `harelAnomalies` (default on); it is not in Fig. 31.
7. The unnamed components get ids (`timedate`, `sw_nonzero`), and `out` has an
   SCXML initial child that the paper never uses (D-17).
8. Values the paper leaves to "activities" are assumptions: stopwatch counter
   (D-6), alarms-beep display (D-7), power-on values (D-8), field increment rules
   and the independent weekday (D-15).
