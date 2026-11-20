# Statechart spec — Citizen Quartz Multi-Alarm III (Harel 1987)

Source: D. Harel, "Statecharts: A Visual Formalism for Complex Systems",
*Science of Computer Programming* 8 (1987) 231–274, Section 3 and Figs. 8–32,
especially Fig. 31 (full watch).
PDF: https://www.inf.ed.ac.uk/teaching/courses/seoc/2006_2007/resources/statecharts.pdf

This file is the **single source of truth** for `watch.scxml`. The SCXML must be
derivable from it line by line, and every test in `tests/` cites a row ID (`T-xx`).

Confidence tags:
- **[P]** stated in the paper's prose (verified via text extraction)
- **[F]** taken from the figures, reconstructed from memory of Fig. 31 — **verify against the PDF**
- **[A]** assumption made to obtain an executable model; the paper is silent or ambiguous

---

## 1. Events

| SCXML event | Harel notation | Source |
|---|---|---|
| `a.down`, `a.up` | `a`, `ā` (overbar = release) | [P] |
| `b.down`, `b.up` | `b`, `b̄` | [P] |
| `c.down`, `c.up` | `c`, `c̄` | [P] |
| `d.down`, `d.up` | `d`, `d̄` | [P] |
| `tick.second` | internal clock tick (from C++ clock model) | [A] |
| `T.hits.T1` | "T hits T1" — current time equals alarm-1 time | [P] |
| `T.hits.T2` | "T hits T2" | [P] |
| `T.hits.hour` | "T hits whole hour" (chime) | [F] |
| `battery.weakens` | "battery weakens" | [P] |
| `battery.dies` | "battery dies" | [P] |
| `battery.removed` | "battery removed" | [P] |
| `battery.inserted` | "battery inserted" | [P] |
| `timeout.*` | `timeout(entered X, n)` / "n sec in X" — use SCXML `<send delay>` + `<cancel>` | [P] |

`any.button` = any of `a.down | b.down | c.down | d.down` (SCXML: `event="a.down b.down c.down d.down"`).

## 2. Hierarchy (literal nesting)

```
watch                                   (root, XOR)
├── dead                                [P]  default when no battery
└── alive                               [P]  AND-state, 6 orthogonal regions
    ├── main                            (XOR)
    │   ├── displays                    [P]  contains deep history H*
    │   │   ├── regular                 [F]  time/date/update group; beep-test applies here
    │   │   │   ├── time                [P]  default of displays
    │   │   │   ├── date                [P]
    │   │   │   ├── wait                [P]  2-sec hold on c before update
    │   │   │   ├── update              [P]
    │   │   │   │   ├── sec   (default) [F]
    │   │   │   │   ├── 1min            [F]
    │   │   │   │   ├── 10min           [F]
    │   │   │   │   ├── hr              [F]
    │   │   │   │   ├── mon             [F]
    │   │   │   │   ├── date            [F]
    │   │   │   │   ├── day             [F]
    │   │   │   │   ├── year            [F]
    │   │   │   │   └── mode (12/24h)   [A]  verify; drop if not in Fig. 31
    │   │   │   └── beep-test           [P]  entered when b and d held together
    │   │   ├── out                     [F]  wrapper for alarm/chime displays (2-min timeout)
    │   │   │   ├── alarm1              [P]
    │   │   │   │   ├── show (default)
    │   │   │   │   └── update1: hr → 10min → 1min   [F]
    │   │   │   ├── alarm2              [P]
    │   │   │   │   ├── show (default)
    │   │   │   │   └── update2: hr → 10min → 1min   [F]
    │   │   │   └── chime               [P]
    │   │   └── stopwatch               [P]  entered via shallow history H [A]
    │   │       ├── zero (default)      [P]
    │   │       └── nonzero             (AND)
    │   │           ├── display: reg (default) | lap   [P]
    │   │           └── run:     on (default) | off    [P]
    │   └── alarms-beep                 [P]
    │       ├── alarm1-beeps            [P]
    │       ├── alarm2-beeps            [P]
    │       └── both-beep               [P]
    ├── alarm1-status: disabled (default) | enabled   [P]
    ├── alarm2-status: disabled (default) | enabled   [P]
    ├── chime-status:  disabled (default) | enabled   [P]
    │                  enabled = quiet (default) | beep  [F]
    ├── light:         off (default) | on             [P]
    └── power:         ok (default) | blink           [P]
```

SCXML IDs use underscores and full paths, for example `update_10min` and `alarm1_update_hr`, because SCXML IDs are global.

## 3. Transitions

`[In(x)]` corresponds to SCXML `cond="In('x')"`.

### 3.1 Top level
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-01 | dead | battery.inserted | alive (default entry, so history is effectively cleared) | [P] |
| T-02 | alive | battery.dies | dead | [P] |
| T-03 | alive | battery.removed | dead | [P] |
| T-04 | power.ok | battery.weakens | power.blink | [P] |

Harel's `clh(main*)` (clear history when entering dead) has no SCXML equivalent. Because T-01 enters `alive` through its *initial* states rather than a history pseudostate, the observable behaviour is the same. Record this in DESIGN-NOTES. Initial state of `watch`: `dead` [A]. The UI starts with a "Insert battery" action; there is also a `--auto-insert` flag.

### 3.2 Display cycle (button a)
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-10 | time | a.down | alarm1 | [F] |
| T-11 | alarm1 | a.down | alarm2 | [F] |
| T-12 | alarm2 | a.down | chime | [F] |
| T-13 | chime | a.down | stopwatch (H) | [F] |
| T-14 | stopwatch | a.down | time | [F] |
| T-15 | date | a.down | ? — **verify**; assumption: no transition | [A] |

### 3.3 Time / date / update (regular)
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-20 | time | d.down | date | [P] |
| T-21 | date | d.down | time | [P] |
| T-22 | date | 120 s in date (`timeout.date`) | time | [P] |
| T-23 | time | c.down | wait | [P] |
| T-24 | wait | c.up | time | [P] |
| T-25 | wait | 2 s in wait (`timeout.wait`) | update (→ sec) | [P] |
| T-26 | update.sec → 1min → 10min → hr → mon → date → day → year → mode → sec | c.down | next field | [F]/[A] order must be verified |
| T-27 | update.* | d.down | self (increment field; internal transition, no exit) | [P] (paper defers the effect to a lower level) |
| T-28 | update | b.down | time | [P] |
| T-29 | update | 120 s with no button | time | [A] |

### 3.4 Alarm / chime displays
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-30 | alarm1.show | c.down | alarm1.update1.hr | [F] |
| T-31 | update1.hr → 10min → 1min | c.down | next field | [F] |
| T-32 | update1.1min | c.down | alarm1.show | [F] |
| T-33 | update1.* | d.down | self (increment; internal) | [F] |
| T-34 | alarm1.update1 | b.down | alarm1.show | [F] |
| T-35..39 | alarm2 | (T-30..T-34 with 2 in place of 1) | | [F] |
| T-40 | out | 120 s with no button pressed | time | [P] ("2 min" return) |
| T-41 | alarm1-status.disabled ↔ enabled | d.down [In('alarm1_show')] | toggle | [P] |
| T-42 | alarm2-status.disabled ↔ enabled | d.down [In('alarm2_show')] | toggle | [P] |
| T-43 | chime-status.disabled ↔ enabled | d.down [In('chime')] | toggle | [P] |

Restart the 120 s timer on any button event. Implement it as `<cancel>` plus a re-sent `<send>` in an internal transition on `a.down b.down c.down d.down` inside `out`.

### 3.5 Stopwatch
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-50 | zero | b.down | nonzero (reg, on) | [P] |
| T-51 | run.on | b.down | run.off | [P] |
| T-52 | run.off | b.down | run.on | [P] |
| T-53 | display.reg | d.down [In('run_on')] | display.lap | [P] |
| T-54 | display.lap | d.down | display.reg | [P] |
| T-55 | nonzero | d.down [In('run_off') && In('disp_reg')] | zero | [P] |
| T-56 | entering/leaving run.on or run.off | — | action: short beep (`beep.short`) | [P] (the half-second beep on start/stop) |

Counting continues while another display is shown. The C++ `StopwatchModel` runs off `run.on` *activity*, not off display visibility [A]. Re-entry uses shallow history [A].

### 3.6 Alarms-beep
Conditions [P]:
- `P1 = In('alarm1_enabled') && (In('alarm2_disabled') || T1 != T2)`
- `P2 = In('alarm2_enabled') && (In('alarm1_disabled') || T1 != T2)`
- `P  = In('alarm1_enabled') && In('alarm2_enabled') && T1 == T2`

| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-60 | displays | T.hits.T1 [P1] | alarm1-beeps | [P] |
| T-61 | displays | T.hits.T2 [P2] | alarm2-beeps | [P] |
| T-62 | displays | T.hits.T1 [P] | both-beep | [P] |
| T-63 | alarms-beep | any.button | displays (H*) | [P] |
| T-64 | alarms-beep | 30 s in alarms-beep | displays (H*) | [P] |

When T1 == T2 the C++ clock emits both `T.hits.T1` and `T.hits.T2`. Only one alarms-beep transition can fire, because the machine is already in alarms-beep when the second event arrives.

### 3.7 Chime, light, beep-test
| ID | From | Event [cond] | To | Src |
|---|---|---|---|---|
| T-70 | chime-status.enabled.quiet | T.hits.hour | enabled.beep | [F] |
| T-71 | enabled.beep | 2 s in beep | enabled.quiet | [F] |
| T-72 | light.off | b.down | light.on | [P] |
| T-73 | light.on | b.up | light.off | [P] |
| T-74 | regular.{time,date,update.*} | d.down [bHeld] or b.down [dHeld] | beep-test | [P] |
| T-75 | beep-test | b.up or d.up | regular (H, shallow) | [P] |
| T-76 | beep-test | — | not reachable from wait, stopwatch, out, alarms-beep, dead | [P] |

`bHeld`/`dHeld` are datamodel booleans that the root updates on every `*.down`/`*.up`, using a root-level internal transition with `<assign>`.

Because the light (T-72) and display regions are orthogonal, `b.down` in update both switches the light on and leaves update. Harel calls this out explicitly [P], and a test must check it.

Fig. 32 anomaly: pressing `a` during beep-test silences the beep until `a.up` [P]. Model this as an optional `beep-test.muted` substate behind a feature flag `harelAnomalies`, default on.

## 4. What each state shows on the LCD [A] (visual only)

| State | Main digits | Top-left | Indicators |
|---|---|---|---|
| time | `HH:MM` big, `SS` small | weekday | alarm1/alarm2/chime icons if enabled; "PM"/"24H" |
| date | `MM-DD` | weekday | — |
| wait | as time | — | — |
| update.X | as time/date, field X blinks at 2 Hz | — | — |
| alarm1/alarm2 | `HH:MM` of T1/T2 | "AL1"/"AL2" | ON/OFF |
| chime | `--:00` | "CH" | ON/OFF |
| stopwatch | `MM'SS"cc` (reg = running value, lap = frozen) | "ST" | "LAP" when lap |
| alarms-beep | as previous display, whole LCD flashes | — | bell icon(s) |
| power.blink | whole LCD blinks at 1 Hz | — | — |
| light.on | backlight (green-blue glow) | — | — |
| dead | blank | — | — |
