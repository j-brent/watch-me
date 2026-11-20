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

- **D-1 Qt version.** `download.qt.io` (and so `aqtinstall`) is blocked by this
  environment's network policy (HTTP 403 from the proxy). The Ubuntu 24.04 distro
  packages give **Qt 6.4.2** (`qt6-base-dev`, `qt6-declarative-dev`,
  `qt6-scxml-dev`, `qml6-module-qtquick-shapes`, …). The project requires
  Qt ≥ 6.4 and avoids APIs newer than that.
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
- **D-6 Stopwatch counter.** Driven by `qt:signal` events on transitions
  (`sw.start` on T-50/T-52, `sw.stop` on T-51, `sw.reset` on T-55, `sw.lap` on T-53)
  rather than by `run.on` being active, because `run.on` is not active while
  another display is shown, but the stopwatch keeps counting.
- **D-7 alarms-beep display.** The paper notes "the displays do not change while
  the alarm is beeping" but defers it to activities. The LCD shows the time display
  with the whole LCD flashing.
- **D-8 Battery insertion.** On `alive` entry the C++ model resets alarms (T1 = T2 =
  12:00), the stopwatch and 12/24h mode to defaults, and seeds the clock from the
  host clock. [A] The paper is silent.
- **D-9 Timeouts and simulation speed.** SCXML delays scale with `timeScale`
  (tests: 0.001). The sim panel's clock-speed slider affects only the simulated
  clock (T, alarms, chime), not the user-interaction timeouts.
