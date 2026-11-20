// `watch` (docs/HOST-CONTRACT.md §3): values only — the simulated clock T, the
// alarm times T1/T2, the stopwatch counter and the 12/24h flag. No modes.
// Port of cpp/src/WatchModel.cpp.
//
// SCXML -> host: model.reset, model.increment{field}, sw.start/stop/lap/reset, beep.short
// host -> SCXML: T.hits.T1, T.hits.T2, T.hits.hour, host.alarms{T1, T2}
//
// T is kept as milliseconds since 1970-01-01 in UTC: a wall clock without a time
// zone, so adding time never jumps over a daylight-saving gap.
import QtQml
import QtScxml

QtObject {
    id: watch
    property QtObject machine
    property var resetTime: null         // ms (UTC wall clock) for model.reset; null: the host's clock

    property real nowMs: Date.UTC(1987, 5, 1, 12, 0, 0)
    property int weekdayOffset: 0        // the 'day' update field sets the weekday on its own
    property bool is24h: false
    property int t1: 12 * 60             // minutes after midnight
    property int t2: 12 * 60
    property real swAccumMs: 0           // simulated ms of the finished run segments
    property real swStartMs: 0           // simulated ms at the last start
    property real simMs: 0               // simulated ms since start-up (monotonic)
    property real lapCentis: 0
    property bool stopwatchRunning: false
    property real speed: 1.0             // 0 … 3600
    property bool running: false         // follow real time (not --manual-time)

    readonly property var _date: new Date(nowMs)
    readonly property int hour: _date.getUTCHours()
    readonly property int minute: _date.getUTCMinutes()
    readonly property int second: _date.getUTCSeconds()
    readonly property int year: _date.getUTCFullYear()
    readonly property int month: _date.getUTCMonth() + 1
    readonly property int day: _date.getUTCDate()
    // 1 = Monday … 7 = Sunday (getUTCDay: 0 = Sunday)
    readonly property int weekday: ((_date.getUTCDay() + 6) % 7 + weekdayOffset) % 7 + 1
    readonly property real stopwatchCentis: Math.floor((swAccumMs + (stopwatchRunning ? simMs - swStartMs : 0)) / 10)

    signal shortBeep()                   // T-56
    signal hit(string event)             // T.hits.* as submitted

    onSpeedChanged: {
        const s = Math.min(3600, Math.max(0, speed))
        if (s !== speed)
            speed = s
    }

    // ---- helpers (all UTC)
    function _daysInMonth(y, m) { return new Date(Date.UTC(y, m, 0)).getUTCDate() }   // m: 1-12
    function _wall(y, mo, d, h, mi, s, ms) { return Date.UTC(y, mo - 1, d, h, mi, s, ms || 0) }
    function _fields() {
        const d = new Date(nowMs)
        return { y: d.getUTCFullYear(), mo: d.getUTCMonth() + 1, d: d.getUTCDate(), h: d.getUTCHours(),
                 mi: d.getUTCMinutes(), s: d.getUTCSeconds(), ms: d.getUTCMilliseconds() }
    }
    function _clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)) }
    // increment one decimal digit of value (place 1 or 10) modulo `modulo`, no carry
    function _incDigit(value, place, modulo) {
        const digit = Math.floor(value / place) % 10
        return value - digit * place + ((digit + 1) % modulo) * place
    }

    // Move T forward by ms. For every whole minute crossed in (before, now]:
    // T.hits.T1, T.hits.T2, T.hits.hour, in that order.
    function advance(ms) {
        ms = Math.floor(ms)
        if (!(ms > 0))
            return
        const before = nowMs
        const after = before + ms
        simMs += ms
        nowMs = after
        for (let t = Math.floor(before / 60000) * 60000 + 60000; t <= after; t += 60000) {
            const minuteOfDay = Math.floor(t / 60000) % 1440
            if (minuteOfDay === t1) _submit("T.hits.T1")
            if (minuteOfDay === t2) _submit("T.hits.T2")
            if (minuteOfDay % 60 === 0) _submit("T.hits.hour")
        }
    }
    function _submit(event) {
        hit(event)
        if (machine)
            machine.submitEvent(event)
    }

    // Sim panel setters. Clamped; no T.hits.*.
    function setTime(h, m, s) {
        const f = _fields()
        nowMs = _wall(f.y, f.mo, f.d, _clamp(h, 0, 23), _clamp(m, 0, 59), _clamp(s, 0, 59))
    }
    function setDate(y, mo, d) {
        const f = _fields()
        mo = _clamp(mo, 1, 12)
        nowMs = _wall(y, mo, _clamp(d, 1, _daysInMonth(y, mo)), f.h, f.mi, f.s, f.ms)
    }
    function _setDateTime(ms) {          // whole seconds, like QTime(h, m, s)
        nowMs = Math.floor(ms / 1000) * 1000
    }
    // Minutes after midnight, modulo 1440 (-1 is 23:59).
    function setAlarm(which, minutes) {
        minutes = ((minutes % 1440) + 1440) % 1440
        if (which === 2) t2 = minutes; else t1 = minutes
        _pushAlarms()
    }
    // T := the next occurrence of T<which> minus `seconds` (today if that moment
    // is not in the past, otherwise tomorrow).
    function jumpBeforeAlarm(which, seconds) {
        const target = which === 2 ? t2 : t1
        const f = _fields()
        let t = _wall(f.y, f.mo, f.d, Math.floor(target / 60), target % 60, 0)
        if (t - seconds * 1000 < nowMs)
            t += 86400000
        _setDateTime(t - seconds * 1000)
    }

    // Host boundary (D-24): the statechart assigns T1/T2 on host.alarms. Queued like
    // every input, so a later T.hits.* sees the new values.
    function _pushAlarms() {
        if (!machine || !machine.initialized)
            return
        machine.submitEvent("host.alarms", { T1: t1, T2: t2 })
    }

    // Power-on defaults (DESIGN-NOTES D-8).
    function reset() {
        if (resetTime !== null) {
            _setDateTime(resetTime)
        } else {
            const d = new Date()                 // the host's local wall clock
            _setDateTime(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate(), d.getHours(),
                                  d.getMinutes(), d.getSeconds()))
        }
        weekdayOffset = 0
        is24h = false
        t1 = 12 * 60
        t2 = 12 * 60
        swAccumMs = 0
        lapCentis = 0
        stopwatchRunning = false
        _pushAlarms()
    }

    function increment(field) {
        const f = _fields()
        let y = f.y, mo = f.mo, d = f.d, h = f.h, mi = f.mi, s = f.s, ms = 0   // QTime(h, m, s): no ms
        const clampDay = () => { d = Math.min(d, _daysInMonth(y, mo)) }
        if (field === "update_sec") {
            s = 0
        } else if (field === "update_1min") {
            mi = _incDigit(mi, 1, 10)
        } else if (field === "update_10min") {
            mi = _incDigit(mi, 10, 6)
        } else if (field === "update_hr") {
            h = (h + 1) % 24
        } else if (field === "update_mon") {
            mo = mo % 12 + 1; clampDay(); ms = f.ms
        } else if (field === "update_date") {
            d = d % _daysInMonth(y, mo) + 1; ms = f.ms
        } else if (field === "update_day") {
            weekdayOffset = (weekdayOffset + 1) % 7
            return
        } else if (field === "update_year") {
            y = y >= 2099 ? 1980 : y + 1; clampDay(); ms = f.ms
        } else if (field === "update_mode") {
            is24h = !is24h
            return
        } else if (/^update[12]_/.test(field)) {
            const two = field.startsWith("update2_")
            const a = two ? t2 : t1
            let ah = Math.floor(a / 60), am = a % 60
            if (field.endsWith("_hr")) ah = (ah + 1) % 24
            else if (field.endsWith("_10min")) am = _incDigit(am, 10, 6)
            else if (field.endsWith("_1min")) am = _incDigit(am, 1, 10)
            if (two) t2 = ah * 60 + am; else t1 = ah * 60 + am
            _pushAlarms()
            return
        } else {
            console.warn("WatchModel: unknown field '" + field + "'")
            return
        }
        nowMs = _wall(y, mo, d, h, mi, s, ms)
    }

    // Outgoing events of watch.scxml (D-11).
    // https://doc.qt.io/qt-6/qml-qtscxml-eventconnection.html
    property EventConnection _events: EventConnection {
        stateMachine: watch.machine
        events: ["model.reset", "model.increment", "sw.start", "sw.stop", "sw.lap", "sw.reset", "beep.short"]
        onOccurred: (event) => {
            switch (event.name) {
            case "model.reset": watch.reset(); break
            case "model.increment": watch.increment(String((event.data || {}).field)); break
            case "sw.start":
                if (!watch.stopwatchRunning) { watch.swStartMs = watch.simMs; watch.stopwatchRunning = true }
                break
            case "sw.stop":
                if (watch.stopwatchRunning) {
                    watch.swAccumMs += watch.simMs - watch.swStartMs
                    watch.stopwatchRunning = false
                }
                break
            case "sw.lap": watch.lapCentis = watch.stopwatchCentis; break
            case "sw.reset": watch.swAccumMs = 0; watch.lapCentis = 0; watch.stopwatchRunning = false; break
            case "beep.short": watch.shortBeep(); break
            }
        }
    }
    property Connections _init: Connections {
        target: watch.machine
        function onInitializedChanged() { watch._pushAlarms() }
    }

    // Real time × speed, about every 20 ms, keeping the fractions.
    property real _lastReal: 0
    property real _carry: 0
    property Timer _timer: Timer {
        interval: 20
        repeat: true
        running: watch.running
        onRunningChanged: if (running) { watch._lastReal = Date.now(); watch._carry = 0 }
        onTriggered: {
            const nowReal = Date.now()
            const sim = (nowReal - watch._lastReal) * watch.speed + watch._carry
            watch._lastReal = nowReal
            const whole = Math.floor(sim)
            watch._carry = sim - whole
            if (whole > 0)
                watch.advance(whole)
        }
    }
}
