# Host contract

What every host must provide so that the shared statechart
(`shared/statechart/watch.scxml`) and the shared UI (`shared/qml/`) run unchanged.
A *host* is one implementation of the glue in one language: `cpp/` is the
reference, and `python/`, `rust/`, `qmljs/` and `qmlts/` follow it
(DESIGN-NOTES D-25). The C++ sources in `cpp/src/` are the executable form of this
document; where the two disagree, fix one of them.

A host passes when `tools/verify.py --cmd "<its start command>"` reports every
row ✓ (§6). That walk drives the app through its command line and checks the
exported event log: each expected entry (§4) must appear within some log line,
and the `# active:` line must contain the expected states. It does not compare
whole lines, so it cannot catch extra text in a line; write §4's formats exactly
anyway, they are the contract.

## 1. What is shared and what each host owns

| Shared, used as is | Owned by each host |
|---|---|
| `shared/statechart/watch.scxml`: all behaviour | Loading and starting the state machine |
| `shared/statechart/layout.json`: view hints | `WatchModel`: simulated clock, alarm times, stopwatch |
| `shared/qml/*`: the whole UI, `Main.qml` is the root | `TimeoutService`: the clock behind "n sec in X" |
| `docs/STATECHART-SPEC.md`: read for the confidence badges | `ButtonRouter`: buttons and other input |
| `tools/verify.py`: the conformance walk | `TransitionTracer`: active set, flashes, event log |
| | `ScxmlTreeModel`, `SpecTags`, `Beeper` |
| | The command line (§5) |

Rules carried over from `CLAUDE.md`: all mode logic lives in the SCXML. A host
keeps values, turns time into events and renders nothing itself. A host never
decides what state the watch is in.

## 2. The SCXML boundary

The machine is created from `watch.scxml` with Qt SCXML and the ECMAScript
datamodel (compiled with `qt_add_statecharts` or loaded at run time with
`QScxmlStateMachine::fromFile`/`StateMachineLoader`; a run-time load must fail
on any `parseErrors()`). Hosts talk to it only through events and the `log`
signal. **Hosts never read or write the datamodel** (D-24).

Events the host submits:

| Event | Data | Sent by |
|---|---|---|
| `a.down` … `d.down`, `a.up` … `d.up` | — | `ButtonRouter` |
| `battery.inserted`, `battery.weakens`, `battery.dies`, `battery.removed`, any other name | — | `ButtonRouter.submit` |
| `T.hits.T1`, `T.hits.T2`, `T.hits.hour` | — | `WatchModel.advance` |
| `timeout.<id>` | `{gen}` (copied from `timer.start`) | `TimeoutService.advance` |
| `host.alarms` | `{T1, T2}` minutes after midnight | `WatchModel`, whenever `t1`/`t2` change (and once the machine is initialized) |
| `host.anomalies` | `{value: bool}` | `ButtonRouter.setDataValue("harelAnomalies", v)` |

Events the machine sends (observe with `connectToEvent` or `EventConnection`):

| Event | Data | Handled by |
|---|---|---|
| `model.reset` | — | `WatchModel.reset()` |
| `model.increment` | `{field}` | `WatchModel.increment(field)` |
| `sw.start`, `sw.stop`, `sw.lap`, `sw.reset` | — | `WatchModel` stopwatch |
| `beep.short` | — | `WatchModel` emits `shortBeep`; the host connects it to `Beeper.shortBeep` |
| `timer.start` | `{id, secs, gen}` | `TimeoutService`: arm `id` to fire `secs` from now |
| `timer.cancel` | `{id}` | `TimeoutService`: disarm `id` |
| `host.data` | `{harelAnomalies, hv_alarm1, hv_alarm2, hv_chime, hv_stopwatch}` | `ButtonRouter` (cache for `dataValue`), `TransitionTracer` (cache for §4.3) |

`QScxmlStateMachine::log(label, msg)`: every spec transition carries
`<log label="T-xx" expr="_event.name"/>`; the tracer turns it into a log line (§4).

## 3. `Main.qml` and the host objects

The host creates `shared/qml/Main.qml` (an `ApplicationWindow`) with these
**required properties**. The other QML files reach them by name through
`Main`'s context. Only the members listed here are used by the QML; a host may
have more.

### `tree` — ScxmlTreeModel
Parses `watch.scxml` itself (an XML reader, not Qt SCXML) into a plain tree.
- `root`: map for the `<scxml>` element: `{id: <scxml name>, kind: "root", initial, children: [...]}`.
  Every node: `{id, kind, historyType, parent, initial, depth, children: [node…], transitions: [transition…]}`;
  `kind` ∈ `root | compound | parallel | atomic | history | final`; `historyType` ∈ `shallow | deep` for history nodes.
  A state without an `id` gets a generated one.
- `transitionsFrom(id)`, `transitionsInto(id)` → list of
  `{source, event, cond, targets: [...], type: "external"|"internal", specId}`.
  `specId` comes from the `<log label="T-xx">` inside the transition, else from
  a `<!-- T-xx -->` comment right after it.
- Not transitions: the default transition inside a `<history>` (D-22) and any
  transition whose events are all `host.*` events (D-24).
- Also used by the tracer: `ancestors(id)` (nearest first) and the transition list.

### `tracer` — TransitionTracer
- `activeSet`: map `stateId → true` for `activeStateNames(false)`; updated on
  `reachedStableState` and `runningChanged`.
- `flashSet`: map `stateId → timestamp` (any value that changes when the state
  flashes again). Every state newly entered in a macrostep, and the source and
  targets of every fired T-xx (§4.3), get a new stamp.
- `entries`: list of log lines (§4), at most the last 500.
- `clearLog()`, `exportLog(path) → bool` (§4.4).

### `buttons` — ButtonRouter
- `held`: map `button → true` for the buttons currently held; signal `heldChanged`.
- `press(b)`, `release(b)`: `b` ∈ `a`–`d` (case-insensitive; anything else is
  ignored). `press` of a held button and `release` of an unheld one do nothing,
  so key auto-repeat never sends a second `.down`. Each real change logs
  `▶ b.down`/`▶ b.up` and submits it.
- `releaseAll()`: `release` every held button.
- `submit(event)`: logs `▶ event` and submits it.
- `dataValue(name)`: the value of `name` in the last `host.data` (undefined before the first).
- `setDataValue(name, value)`: only `"harelAnomalies"` → submit `host.anomalies{value}`.
  Nothing changes until the machine answers with `host.data`.
- Signal `dataValueChanged(name)`: emitted for every key whose value differs from
  the previous `host.data`; `""` for the first `host.data` (everything is new).

### `watch` — WatchModel
Properties: `hour`, `minute`, `second`, `year`, `month`, `day`, `weekday`
(1 = Monday … 7 = Sunday), `is24h`, `t1`, `t2` (minutes after midnight),
`stopwatchCentis`, `lapCentis`, `stopwatchRunning`, `speed` (writable, 0–3600).
Signal `shortBeep`. Methods `setTime(h, m, s)`, `jumpBeforeAlarm(which, secs)`.
Behaviour (§5.2 for the time steps):
- The clock is a wall clock without a time zone (use UTC arithmetic). Start
  value 1987-06-01 12:00:00 until the first `model.reset`.
- `advance(ms)`: move T forward. For every whole minute crossed in `(before, now]`
  submit, in this order, `T.hits.T1` if it equals `t1`, `T.hits.T2` if it equals
  `t2`, `T.hits.hour` if the minute is 0.
- Running (not `--manual-time`): advance by real elapsed time × `speed`, about every 20 ms, keeping fractions.
- `setTime(h, m, s)`: clamp to 0–23 / 0–59 / 0–59, keep the date, no `T.hits`.
- `setAlarm(which, minutes)`: minutes modulo 1440 (−1 is 23:59), then `host.alarms`.
- `jumpBeforeAlarm(which, secs)`: T := next occurrence of `t<which>` − `secs`
  (today if that moment is not in the past, else tomorrow).
- `model.reset`: T := the `--time` value (else the host's current time),
  weekday offset 0, 12-hour mode, `t1 = t2 = 12:00`, stopwatch zero and stopped, then `host.alarms`.
- `model.increment{field}`:
  `update_sec` seconds := 0; `update_1min` / `update_10min` add 1 to that minute
  digit without carry (mod 10 / mod 6); `update_hr` hour + 1 mod 24;
  `update_mon` month + 1 (12 → 1), day clamped to the month's length;
  `update_date` day + 1, wrapping to 1 after the month's last day;
  `update_day` weekday offset + 1 mod 7 (the date does not change);
  `update_year` year + 1, 2099 → 1980, day clamped; `update_mode` toggles `is24h`;
  `update1_*` / `update2_*` (`hr`, `10min`, `1min`) change `t1` / `t2` the same
  way, then `host.alarms`. Unknown fields are ignored with a warning.
- Stopwatch (simulated time, ms internally, `stopwatchCentis` = ms / 10):
  `sw.start` starts if stopped; `sw.stop` adds the running segment and stops;
  `sw.lap` sets `lapCentis` to the current value; `sw.reset` zeroes both and stops.

### `beeper` — Beeper
`enabled` (read/write, default false), `available` (true if the host can play
sound), `continuous` (written by QML: a beeping state is active), `shortBeep()`
(T-56's half-second beep). A host without audio sets `available = false` and does nothing.

### `specTags` — SpecTags
Reads `docs/STATECHART-SPEC.md`: every table row `| T-xx | … [P|F|A] …` gives
`T-xx → P|F|A`. `weakest(ids)` → the weakest tag among `ids` (`A` < `F` < `P`),
`""` if none is known.

### `appLayout`
`shared/statechart/layout.json`, parsed to a plain object.

## 4. The event log

One string per entry, in exactly these formats. The walk (§6) looks for the
expected entries as substrings of these lines.

### 4.1 Inputs
`▶ <event>` for every event submitted through `ButtonRouter` (not for
`T.hits.*`, `timeout.*` or `host.*`).

### 4.2 Outgoing events
`◀ <name>` followed by ` key=value` for each data field in key order, for every
`model.*`, `sw.*` and `beep.*` event, e.g. `◀ model.increment field=update_mon`.

### 4.3 Fired transitions
For every `log(label, msg)` whose label is `T-<digits>`:
`<label> <msg or "·">: <source> → <targets joined by ", " or "(no state change)">`,
e.g. `T-10 a.down: time → alarm1`, `T-27 d.down: update → (no state change)`.

Source and targets come from the parsed tree, because the log signal only names
the row (and Qt may deliver it after the macrostep, D-18):
1. Keep the last 4 *distinct* stable configurations (newest first): when a
   macrostep ends in the same configuration as the newest one kept (`host.data`,
   `timer.*`, `model.*` and the like change no state), do not add it again
   (DESIGN-NOTES D-26).
2. Candidates: transitions with this `specId` whose source was active in one of
   them and whose every target is active now (for a history target: its parent).
   If there are none, drop the "active now" condition.
3. If several remain (the clh(main\*) pairs: `[hv_x] → x_H` and `→ x`), take the
   one whose condition is a plain identifier that is true in the last
   `host.data`; otherwise the one without a condition; otherwise the first.
4. Stamp the source and targets in `flashSet`.

### 4.4 Export
`exportLog(path)`: strip a leading `file://`, write the entries joined by `\n`,
then `\n# active: ` followed by the active state names (`activeStateNames(false)`)
joined by spaces, then `\n`. UTF-8. Returns false if the file cannot be written.

## 5. Command line

Each host starts with the same options:

| Option | Meaning |
|---|---|
| `--no-gui` | No window and no QML: statechart, clock, input, log and `--script` only (a core application). |
| `--auto-insert` | Submit `battery.inserted` (through `ButtonRouter.submit`) right after start. |
| `--time ISO` | `model.reset` sets T to this value, e.g. `1987-06-01T10:08:30`. |
| `--manual-time` | Neither the watch clock nor the timeout clock follows real time; only script steps move them. |
| `--script STEPS` | Steps separated by `;`, run after start-up (§5.1). |
| `--exit` | Quit when the script has run (after `--grab-delay`). |
| `--grab FILE`, `--grab-delay MS` (800) | Save a screenshot of the window, then quit. GUI only. |
| `--size WxH` | Resize the window. GUI only. |

Hosts start the machine, then run the first step 200 ms later, and each later
step 60 ms after the previous one (or after `wait:ms`).

### 5.1 Script steps

| Step | Effect |
|---|---|
| `a` (one letter) | `press(a)`, then `release(a)` 30 ms later |
| `+a` / `-a` | `press(a)` / `release(a)` |
| `wait:ms` | the next step runs `ms` later |
| `alarm:n:HH:MM` | `setAlarm(n, HH·60 + MM)` |
| `jump:n:s` | `jumpBeforeAlarm(n, s)` |
| `settime:H:M:S` | `setTime(H, M, S)` |
| `clock:ms` | `WatchModel.advance(ms)` (the watch time T) |
| `advance:ms` | `TimeoutService.advance(ms)` (the timeout clock, §5.2) |
| `speed:x` | `watch.speed = x` |
| `set:name:value` | `setDataValue(name, value)`; `true`/`false` are booleans, anything else a number |
| `export:path` | `exportLog(path)`; the path is the rest of the step (Windows `C:\…` works) |
| `select:id` | GUI only: set `selected` on the object named `statechartView` |
| anything else | `submit(step)` |

### 5.2 Timeouts
`TimeoutService` keeps its own clock `now` (ms) and a table `id → {due, gen}`.
`timer.start{id, secs, gen}` sets `due = now + round(secs × 1000)`;
`timer.cancel{id}` removes `id`. `advance(ms)`:
1. First let the machine process everything already submitted (so a timer just
   started by a button counts from the current `now`).
2. Repeat: take the earliest pending deadline `≤ now + ms` (ties: smaller `id`);
   set `now` to it, remove it, submit `timeout.<id>{gen}`, and let the machine
   process it before looking again (a timer started by that timeout can fire in
   the same step, D-21).
3. Set `now` to the end of the step. `advance` does not re-enter itself.

Running (not `--manual-time`) means `advance(elapsed real ms)` about every 20 ms.

## 6. Conformance

```sh
python3 tools/verify.py --cmd "<the host's start command>"
```

It runs the app once per spec row and scenario (68 runs), each with
`--no-gui --time 1987-06-01T10:08:30 --manual-time --exit --script "battery.inserted;…;export:<file>"`,
and checks the exported log and `# active:` line. A host passes when it prints
`68/68 ok; uncovered: []`. Only the C++ host writes or checks
`docs/VERIFICATION.md`.
