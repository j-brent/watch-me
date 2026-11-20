# Statechart spec — Citizen Quartz Multi-Alarm III (Harel 1987)

Source: D. Harel, "Statecharts: A Visual Formalism for Complex Systems",
*Science of Computer Programming* 8 (1987) 231–274, Section 3 and Figs. 8–32,
especially Fig. 31 (full watch). Local copy: `docs/harel1987.pdf`
(journal page *n* = PDF page *n* − 230; Fig. 31 is the fold-out on PDF pp. 21–22).

This file is the **single source of truth** for `watch.scxml`. The SCXML must be
derivable from it line by line, and every test in `tests/` cites a row ID (`T-xx`).

Confidence tags:
- **[P]** confirmed against the paper — its prose and/or its figures (Step 0, see
  `docs/DESIGN-NOTES.md` for the figure each row was checked against)
- **[F]** taken from the figures but not verified (none remain after Step 0)
- **[A]** assumption made to obtain an executable model; the paper is silent or ambiguous

Step 0 (verification against the scanned PDF) was completed; every row that changed
is logged in `docs/DESIGN-NOTES.md` §1 as *row → old → new → figure*.

---

## 1. Events

| SCXML event | Harel notation | Source |
|---|---|---|
| `a.down`, `a.up` | `a`, `â` (Harel draws release as a hat/overbar) | [P] §3, Figs. 13, 29, 32 |
| `b.down`, `b.up` | `b`, `b̂` | [P] |
| `c.down`, `c.up` | `c`, `ĉ` | [P] |
| `d.down`, `d.up` | `d`, `d̂` | [P] |
| `T.hits.T1` | "T hits T1" — current time reaches alarm-1 time | [P] Figs. 8, 31 |
| `T.hits.T2` | "T hits T2" | [P] Figs. 8, 31 |
| `T.hits.hour` | "T is whole hour" (chime) | [P] Figs. 28, 31 |
| `battery.weakens` | "battery weakens" | [P] Figs. 28, 31 |
| `battery.dies` | "weak battery dies" | [P] Figs. 26, 28, 31 |
| `battery.removed` | "battery removed" | [P] |
| `battery.inserted` | "battery inserted" | [P] |
| `timeout.*` | `timeout(entered X, n)` / "n sec in X" (§4.2) — X's entry sends `timer.start{id, secs, gen}`, its exit `timer.cancel{id}`; the clock (C++ `TimeoutService`) answers `timeout.<id>{gen}`, taken only if `gen` is current (DESIGN-NOTES D-20) | [P] |

`any button pressed` = any of `a.down | b.down | c.down | d.down`
(SCXML: `event="a.down b.down c.down d.down"`).

There is no `tick.second` event: the per-second display refresh is a C++ property
change, not a statechart event (nothing in Fig. 31 reacts to it). [A]

## 2. Hierarchy (literal nesting, as in Fig. 31)

```
watch                                   (root, XOR)                          [P] Fig. 26/28/31
├── dead                                default                              [P] Fig. 26/28
└── alive                               AND-state, 6 orthogonal components   [P] Fig. 27
    ├── main                            (XOR) default → displays             [P]
    │   ├── displays                    H* (deep), default → regular         [P] Fig. 31
    │   │   ├── regular                 AND-state (box drawn around time/date/update) [P] Fig. 31
    │   │   │   ├── (timedate)          unnamed XOR component, default → time
    │   │   │   │   ├── time            default                              [P]
    │   │   │   │   ├── date                                                 [P]
    │   │   │   │   └── update          entered at sec                       [P] Fig. 14/31
    │   │   │   │       sec, 1min, 10min, hr, mon, date, day, year, mode     [P] Fig. 14/31
    │   │   │   └── beep-test           XOR, default → 00                    [P] Fig. 29/31
    │   │   │       00, 10, 01, beep (beep = on | off, Fig. 32, flag)        [P]
    │   │   ├── wait                    outside `regular` (no beep-test)     [P] Fig. 31 + prose p. 249
    │   │   ├── out                     wrapper with the 2-min return        [P] Fig. 31
    │   │   │   ├── alarm1              H → off | on                         [P] Fig. 13/31
    │   │   │   ├── update1             hr → 10min → 1min, H(d)              [P] Fig. 15/31
    │   │   │   ├── alarm2              H → off | on                         [P]
    │   │   │   ├── update2             hr → 10min → 1min, H(d)              [P]
    │   │   │   └── chime               H → off | on                         [P]
    │   │   └── stopwatch               H* (deep), default → zero            [P] Fig. 25/31
    │   │       ├── zero
    │   │       └── (nonzero)           unnamed AND-state
    │   │           ├── display: reg (default) | lap
    │   │           └── run:     on (default) | off
    │   └── alarms-beep                                                      [P] Fig. 8/31
    │       ├── alarm1-beeps
    │       ├── alarm2-beeps
    │       └── both-beep
    ├── alarm1-status: disabled (default) | enabled                          [P] Fig. 28/31
    ├── alarm2-status: disabled (default) | enabled                          [P]
    ├── chime-status:  disabled (default) | enabled = quiet (default) | beep [P] Fig. 28/31
    ├── light:         off (default) | on                                    [P]
    └── power:         OK (default) | blink                                  [P]
```

The 12/24h `mode` field exists (Fig. 14, Fig. 31; "am/pm or 24 hour time modes", p. 236).

SCXML IDs are global, so they use underscores and full paths:

| Paper | SCXML id |
|---|---|
| (timedate) | `timedate` |
| update.sec … update.mode | `update_sec`, `update_1min`, `update_10min`, `update_hr`, `update_mon`, `update_date`, `update_day`, `update_year`, `update_mode` |
| beep-test 00/10/01/beep (on/off) | `beep_test`, `bt_00`, `bt_10`, `bt_01`, `bt_beep`, `bt_beep_on`, `bt_beep_off` |
| alarm1.off/on, H | `alarm1_off`, `alarm1_on`, `alarm1_H` |
| update1.hr/10min/1min | `update1_hr`, `update1_10min`, `update1_1min` |
| chime.off/on, H | `chime_off`, `chime_on`, `chime_H` |
| stopwatch, zero, (nonzero), display, run | `stopwatch`, `sw_zero`, `sw_nonzero`, `sw_display`, `sw_run` |
| reg/lap/on/off | `sw_reg`, `sw_lap`, `sw_on`, `sw_off` |
| displays H*, stopwatch H* | `displays_H`, `sw_H` |
| alarms-beep and children | `alarms_beep`, `alarm1_beeps`, `alarm2_beeps`, `both_beep` |
| alarm1-status.disabled/enabled | `alarm1_status`, `alarm1_disabled`, `alarm1_enabled` (same for 2) |
| chime-status | `chime_status`, `chime_disabled`, `chime_enabled`, `chime_quiet`, `chime_beep` |
| light, power | `light`, `light_off`, `light_on`, `power`, `power_ok`, `power_blink` |

## 3. Transitions

`(in x)` in the paper corresponds to SCXML `cond="In('x')"`.

Every row below is a user transition. The default transition inside an SCXML
`<history>` (where an H goes when it has no value yet) is not a row: it is part of
the H connector, never taken on an event, and carries no `T-xx` (DESIGN-NOTES D-22).

### 3.1 Top level
| ID | From | Event [cond] / action | To | Src |
|---|---|---|---|---|
| T-01 | dead | battery.inserted | alive (default entry everywhere) | [P] Fig. 26/28/31 |
| T-02 | power.blink | battery.dies / clh(main*) | dead | [P] Fig. 28/31 ("weak battery dies") |
| T-03 | alive | battery.removed / clh(main*) | dead | [P] Fig. 28/31 |
| T-04 | power.OK | battery.weakens | power.blink | [P] Fig. 28/31 |

**clh(main\*)** — SCXML has no clear-history action. It is emulated with datamodel
booleans `hv_alarm1`, `hv_alarm2`, `hv_chime`, `hv_stopwatch` ("history valid"):
T-02/T-03 set all of them to `false`; each state that owns an H/H\* sets its flag to
`true` on exit; and every transition that enters through an H whose value might
pre-date the last `dead` is split into a `[hv_x]` branch targeting the history
pseudostate and a fallback branch targeting the state's default. The observable
behaviour equals Harel's "history is forgotten if dead has been entered in the
meantime" (p. 247). Initial state of `watch`: `dead` [P] (Fig. 26 default arrow).
The UI starts with an "Insert battery" action; there is also a `--auto-insert` flag.

### 3.2 Display cycle (button a)
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-10 | time | a.down | alarm1 (via its H: last off/on, default off) | [P] Fig. 13/31 |
| T-11 | alarm1 | a.down | alarm2 (via its H) | [P] Fig. 31 |
| T-12 | alarm2 | a.down | chime (via its H) | [P] Fig. 31 |
| T-13 | chime | a.down | stopwatch (H\*, deep) | [P] Fig. 25/31 |
| T-14 | stopwatch | a.down | regular (default → time, beep-test 00) | [P] Fig. 31 |
| T-15 | date, update.\*, update1.\*, update2.\*, wait | a.down | no transition (`a` leaves only time, alarm1, alarm2, chime, stopwatch) | [P] Fig. 31 |

### 3.3 Time / date / update (regular)
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-20 | time | d.down [b up ≡ `In('bt_00') \|\| In('bt_01')`] | date | [P] Fig. 31 ("d(b up)") |
| T-21 | date | d.down | time | [P] |
| T-22 | date | 2 min in date (`timeout.date`, restarted by any button) | time | [P] Fig. 9/31 + prose p. 247 |
| T-23 | time | c.down | wait | [P] |
| T-24 | wait | c.up | time | [P] |
| T-25 | wait | 2 sec in wait (`timeout.wait`) | update.sec | [P] Fig. 14/31 |
| T-26 | update.sec → 1min → 10min → hr → mon → date → day → year → mode; **mode → time** | c.down | next field; from mode leaves update to time | [P] Fig. 14/31 |
| T-27 | update.\* | d.down | self (increment current field; targetless transition) | [P] Fig. 14 (d → H) + prose p. 238 |
| T-28 | update | b.down | time | [P] |
| T-29 | update | 2 min in update (`timeout.update`, restarted by any button) | time | [P] Fig. 31 + prose p. 247 |

### 3.4 Alarm / chime displays (all inside `out`)
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-30 | alarm1 | c.down | update1.hr | [P] Fig. 15/31 |
| T-31 | update1.hr → 10min → 1min | c.down | next field | [P] Fig. 15/31 |
| T-32 | update1.1min | c.down | alarm1 (via H) | [P] Fig. 15/31 |
| T-33 | update1.\* | d.down | self (increment field; targetless) | [P] Fig. 15 (d → H) |
| T-34 | update1 | b.down | alarm1 (via H) | [P] Fig. 15/31 |
| T-35 | alarm2 | c.down | update2.hr | [P] |
| T-36 | update2.hr → 10min → 1min | c.down | next field | [P] |
| T-37 | update2.1min | c.down | alarm2 (via H) | [P] |
| T-38 | update2.\* | d.down | self (increment; targetless) | [P] |
| T-39 | update2 | b.down | alarm2 (via H) | [P] |
| T-40 | out | 2 min in out, no change (`timeout.out`, restarted by any button) | regular (→ time) | [P] Fig. 31 ("2 min in out (no change)") |
| T-41 | alarm1-status.disabled → enabled / enabled → disabled | d.down [In('alarm1_off')] / d.down [In('alarm1_on')] | toggle | [P] Fig. 28/31 |
| T-42 | alarm2-status, same with alarm2 | d.down [In('alarm2_off')] / [In('alarm2_on')] | toggle | [P] |
| T-43 | chime-status.disabled → enabled(quiet) / enabled → disabled | d.down [In('chime_off')] / [In('chime_on')] | toggle | [P] |
| T-44 | alarm1.off ↔ alarm1.on | d.down | toggle (display of the status) | [P] Fig. 10/13/31 |
| T-45 | alarm2.off ↔ alarm2.on | d.down | toggle | [P] |
| T-46 | chime.off ↔ chime.on | d.down | toggle | [P] |
| T-47 | alive (date, update or out active) | any button pressed | self: restart the 2-min timer of the active one | [P] prose p. 247 ("on condition that no button has been depressed in the interim") |

T-41/T-44 fire on the same `d.down` in orthogonal components; the `In()` condition is
evaluated on the configuration *before* the step, so the two stay in lockstep.

### 3.5 Stopwatch
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-50 | zero | b.down | nonzero (reg, on) | [P] Fig. 25/31 |
| T-51 | run.on | b.down | run.off | [P] |
| T-52 | run.off | b.down | run.on | [P] |
| T-53 | display.reg | d.down [In('sw_on')] | display.lap | [P] |
| T-54 | display.lap | d.down | display.reg | [P] |
| T-55 | display.reg | d.down [In('sw_off')] | zero | [P] Fig. 25/31 ("d (in off)" from reg.) |
| T-56 | T-50, T-51, T-52 | action: half-second beep (`beep.short`) + counter start/stop | — | [P] p. 258 ("start-half-second-beep … the two transitions within stopwatch.run") |

Counting continues while another display is shown. The counter is started and
stopped by actions on T-50/T-51/T-52 (`sw.start`/`sw.stop`), reset on entering
`zero` (T-55) and captured on T-53 (`sw.lap`); leaving `stopwatch` by `a` does not
touch it. [A] (the paper defers the activity to Section 5). Re-entry uses deep history
H\* [P] (Fig. 25/31).

### 3.6 Alarms-beep
Conditions [P] (Fig. 8 prose p. 236, made precise on p. 247):
- `P1 = In('alarm1_enabled') && (In('alarm2_disabled') || T1 != T2)`
- `P2 = In('alarm2_enabled') && (In('alarm1_disabled') || T1 != T2)`
- `P  = In('alarm1_enabled') && In('alarm2_enabled') && T1 == T2`

| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-60 | displays | T.hits.T1 [P1] | alarm1-beeps | [P] Fig. 8/31 |
| T-61 | displays | T.hits.T2 [P2] | alarm2-beeps | [P] |
| T-62 | displays | T.hits.T1 [P] | both-beep | [P] |
| T-63 | alarms-beep | any button pressed | displays (H\*) | [P] Fig. 13/31 |
| T-64 | alarms-beep | 30 sec in alarms-beep | displays (H\*) | [P] |

When T1 == T2 the C++ clock emits both `T.hits.T1` and `T.hits.T2`. Only one
alarms-beep transition can fire: if both alarms are enabled, P2 is false; otherwise
the machine is already in alarms-beep when the second event arrives.

### 3.7 Chime, light, beep-test
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-70 | chime-status.enabled.quiet | T.hits.hour | enabled.beep | [P] Fig. 28/31 |
| T-71 | enabled.beep | 2 sec in beep | enabled.quiet | [P] Fig. 28/31 |
| T-72 | light.off | b.down | light.on | [P] |
| T-73 | light.on | b.up | light.off | [P] |
| T-74 | beep-test.00 | b.down | 10 | [P] Fig. 29/31 |
| T-75 | beep-test.00 | d.down | 01 | [P] |
| T-76 | beep-test.10 | d.down | beep | [P] |
| T-77 | beep-test.01 | b.down | beep | [P] |
| T-78 | beep-test.10 | b.up | 00 | [P] |
| T-79 | beep-test.01 | d.up | 00 | [P] |
| T-80 | beep-test.beep | d.up | 10 | [P] |
| T-81 | beep-test.beep | b.up | 01 | [P] |
| T-82 | beep.on | a.down [harelAnomalies && !In('time')] | beep.off | [P] Fig. 32 (`!In('time')`: see note) |
| T-83 | beep.off | a.up | beep.on | [P] Fig. 32 |

Beep-test is an orthogonal component of `regular`, so it is active exactly in
time, date and update.\* and not in wait, out, stopwatch, alarms-beep or dead [P]
(prose p. 249, Fig. 31). Leaving `regular` resets it to 00 on the next entry.

Because the light (T-72) and display components are orthogonal, `b.down` in update
both switches the light on and leaves update [P] (p. 247). The paper's scenario
(p. 250): in `update.mon`, press d then b without releasing → `time`, month advanced
by one, beep-test in `beep`, light on. Both are required tests.

Fig. 32 (pressing `a` silences the beep until `a.up`) is not part of Fig. 31; it is
modelled as `beep.on | beep.off` behind the datamodel flag `harelAnomalies`
(default `true`). With the flag off, T-82 never fires. In `time`, `a` also triggers
T-10, which leaves `regular`; T-10 must win (W3C SCXML picks the transition of the
earlier atomic state in document order, Statemate gives priority to the outer
scope — both select T-10). Qt 6.4 resolves this conflict the other way, so T-82
carries the explicit guard `!In('time')`, which encodes the same priority.

## 4. What each state shows on the LCD [A] (visual only)

| State | Main digits | Top-left | Indicators |
|---|---|---|---|
| time, wait | `HH:MM` big, `SS` small | weekday | alarm1/alarm2/chime icons if enabled; "PM" (12h) or "24H" |
| date | `MM-DD` | weekday | — |
| update.X | time fields (sec/1min/10min/hr/mode) or date fields (mon/date/day/year); field X blinks at 2 Hz | "SET" | — |
| alarm1/alarm2 | `HH:MM` of T1/T2 | "AL1"/"AL2" | "ON"/"OFF" (from alarmN.on/off) |
| update1/update2 | `HH:MM` of T1/T2, field blinks | "AL1"/"AL2" | — |
| chime | `--:00` | "CH" | "ON"/"OFF" |
| stopwatch | `MM'SS"cc` (reg = running value, lap = frozen); after `59'59"99` the display wraps to `00'00"00` while counting goes on | "ST" | "LAP" when lap |
| alarms-beep | the time display; whole LCD flashes | — | bell icon(s) |
| beep-test.beep (on), chime.beep, alarms-beep | beeper indicator active | — | — |
| power.blink | whole LCD blinks at 1 Hz | — | — |
| light.on | backlight (green-blue glow) | — | — |
| dead | blank | — | — |

Hour digits of every `HH:MM` (time, wait, update, alarm1/alarm2, update1/update2,
alarms-beep): in 24-hour mode `00`–`23`, zero-padded (midnight is `00:00`); in
12-hour mode `1`–`12` with a blank instead of a leading zero (midnight and noon are
`12:00`, AM and PM). Implemented in `qml/LcdView.mjs`, tested by `tests/tst_lcd.cpp`.
