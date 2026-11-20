"""WatchModel: values only (HOST-CONTRACT §3 `watch`). The simulated clock T, the
alarm times T1/T2, the stopwatch counter and the 12/24h flag. No modes."""
from __future__ import annotations

import calendar
from datetime import date, datetime, timedelta

from PySide6.QtCore import QElapsedTimer, QObject, Property, Qt, QTimer, Signal, Slot
from PySide6.QtScxml import QScxmlStateMachine

from .scxml_events import connect_to_event, event_data

START = datetime(1987, 6, 1, 12, 0, 0)    # until the first model.reset


def _clamped(y: int, mo: int, d: int) -> date:
    return date(y, mo, min(d, calendar.monthrange(y, mo)[1]))


def _inc_digit(value: int, place: int, modulo: int) -> int:
    """Add 1 to one decimal digit of value (place 1 or 10) modulo `modulo`, no carry."""
    digit = (value // place) % 10
    return value - digit * place + ((digit + 1) % modulo) * place


class WatchModel(QObject):
    timeChanged = Signal()
    settingsChanged = Signal()
    stopwatchChanged = Signal()
    speedChanged = Signal()
    runningChanged = Signal()
    shortBeep = Signal()                   # T-56

    def __init__(self, machine: QScxmlStateMachine, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._machine = machine
        # A wall clock without time zone: naive datetime arithmetic never skips a DST gap.
        self._now = START
        self._reset_time: datetime | None = None
        self._weekday_offset = 0           # the 'day' update field moves the weekday alone
        self._24h = False
        self._t1 = self._t2 = 12 * 60
        self._sw_accum_ms = 0              # finished run segments
        self._sw_start_ms = 0              # _sim_ms at the last start
        self._sim_ms = 0                   # simulated ms since construction (monotonic)
        self._lap = 0
        self._sw_running = False
        self._speed = 1.0
        self._carry = 0.0
        self._last_real = 0
        self._real = QElapsedTimer()
        self._timer = QTimer(self)
        self._timer.setInterval(20)
        self._timer.setTimerType(Qt.TimerType.PreciseTimer)
        self._timer.timeout.connect(self._on_timer)

        # Outgoing events of watch.scxml (DESIGN-NOTES D-11).
        on = lambda spec, fn: connect_to_event(machine, spec, fn, self)
        on("model.reset", lambda e: self.reset())
        on("model.increment", lambda e: self.increment(str(event_data(e).get("field", ""))))
        on("sw.start", lambda e: self._sw_start())
        on("sw.stop", lambda e: self._sw_stop())
        on("sw.lap", lambda e: self._sw_lap())
        on("sw.reset", lambda e: self._sw_reset())
        on("beep.short", lambda e: self.shortBeep.emit())
        machine.initializedChanged.connect(self._push_alarms)

    # --- properties ---------------------------------------------------------
    def _hour(self) -> int: return self._now.hour
    def _minute(self) -> int: return self._now.minute
    def _second(self) -> int: return self._now.second
    def _year(self) -> int: return self._now.year
    def _month(self) -> int: return self._now.month
    def _day(self) -> int: return self._now.day
    def _weekday(self) -> int: return (self._now.isoweekday() - 1 + self._weekday_offset) % 7 + 1
    def _is24h(self) -> bool: return self._24h
    def _get_t1(self) -> int: return self._t1
    def _get_t2(self) -> int: return self._t2
    def _lap_centis(self) -> int: return self._lap
    def _sw_is_running(self) -> bool: return self._sw_running
    def _get_speed(self) -> float: return self._speed

    def stopwatch_centis(self) -> int:
        running = self._sim_ms - self._sw_start_ms if self._sw_running else 0
        return (self._sw_accum_ms + running) // 10

    def set_speed(self, s: float) -> None:
        s = min(max(float(s), 0.0), 3600.0)
        if s != self._speed:
            self._speed = s
            self.speedChanged.emit()

    hour = Property(int, _hour, notify=timeChanged)
    minute = Property(int, _minute, notify=timeChanged)
    second = Property(int, _second, notify=timeChanged)
    year = Property(int, _year, notify=timeChanged)
    month = Property(int, _month, notify=timeChanged)
    day = Property(int, _day, notify=timeChanged)
    weekday = Property(int, _weekday, notify=timeChanged)       # 1 = Monday … 7 = Sunday
    is24h = Property(bool, _is24h, notify=settingsChanged)
    t1 = Property(int, _get_t1, notify=settingsChanged)          # minutes after midnight
    t2 = Property(int, _get_t2, notify=settingsChanged)
    stopwatchCentis = Property(int, stopwatch_centis, notify=stopwatchChanged)
    lapCentis = Property(int, _lap_centis, notify=stopwatchChanged)
    stopwatchRunning = Property(bool, _sw_is_running, notify=stopwatchChanged)
    speed = Property(float, _get_speed, set_speed, notify=speedChanged)

    # --- real-time driver ---------------------------------------------------
    def set_running(self, running: bool) -> None:
        if running == self._timer.isActive():
            return
        if running:
            self._real.start()
            self._last_real = 0
            self._timer.start()
        else:
            self._timer.stop()
        self.runningChanged.emit()

    def _on_timer(self) -> None:
        now_real = self._real.elapsed()
        sim = (now_real - self._last_real) * self._speed + self._carry
        self._last_real = now_real
        whole = int(sim)
        self._carry = sim - whole
        if whole > 0:
            self.advance(whole)

    # --- time ---------------------------------------------------------------
    def advance(self, sim_ms: int) -> None:
        """Move T forward; T.hits.* for every whole minute crossed in (before, now]."""
        if sim_ms <= 0:
            return
        before = self._now
        self._now = before + timedelta(milliseconds=sim_ms)
        self._sim_ms += sim_ms
        t = before.replace(second=0, microsecond=0) + timedelta(minutes=1)
        while t <= self._now:
            mod = t.hour * 60 + t.minute
            if mod == self._t1:
                self._machine.submitEvent("T.hits.T1")
            if mod == self._t2:
                self._machine.submitEvent("T.hits.T2")
            if t.minute == 0:
                self._machine.submitEvent("T.hits.hour")
            t += timedelta(minutes=1)
        if before.replace(microsecond=0) != self._now.replace(microsecond=0):
            self.timeChanged.emit()
        if self._sw_running:
            self.stopwatchChanged.emit()

    def set_reset_time(self, dt: datetime | None) -> None:
        self._reset_time = dt

    def _set_datetime(self, dt: datetime) -> None:
        self._now = dt.replace(microsecond=0, tzinfo=None)
        self.timeChanged.emit()

    @Slot(int, int, int)
    def setTime(self, h: int, m: int, s: int) -> None:
        clamp = lambda v, hi: min(max(int(v), 0), hi)
        self._now = datetime.combine(self._now.date(), datetime.min.time()).replace(
            hour=clamp(h, 23), minute=clamp(m, 59), second=clamp(s, 59))
        self.timeChanged.emit()

    @Slot(int, int)
    def setAlarm(self, which: int, minutes: int) -> None:
        minutes %= 1440                    # Python's % is already non-negative: -1 -> 1439
        if which == 2:
            self._t2 = minutes
        else:
            self._t1 = minutes
        self._push_alarms()
        self.settingsChanged.emit()

    @Slot(int, int)
    def jumpBeforeAlarm(self, which: int, seconds: int) -> None:
        target = self._t2 if which == 2 else self._t1
        t = datetime.combine(self._now.date(), datetime.min.time()) + timedelta(minutes=target)
        if t - timedelta(seconds=seconds) < self._now:
            t += timedelta(days=1)
        self._set_datetime(t - timedelta(seconds=seconds))

    def _push_alarms(self) -> None:
        # Host boundary (DESIGN-NOTES D-24): the statechart assigns T1/T2 on host.alarms.
        if self._machine.isInitialized():
            self._machine.submitEvent("host.alarms", {"T1": self._t1, "T2": self._t2})

    # --- outgoing events ----------------------------------------------------
    def reset(self) -> None:
        self._set_datetime(self._reset_time or datetime.now())
        self._weekday_offset = 0
        self._24h = False
        self._t1 = self._t2 = 12 * 60
        self._sw_accum_ms = 0
        self._lap = 0
        self._sw_running = False
        self._push_alarms()
        self.settingsChanged.emit()
        self.stopwatchChanged.emit()
        self.timeChanged.emit()

    def increment(self, field: str) -> None:
        n = self._now
        if field.startswith(("update1_", "update2_")):
            first = field.startswith("update1_")
            a = self._t1 if first else self._t2
            h, m = divmod(a, 60)
            if field.endswith("_hr"):
                h = (h + 1) % 24
            elif field.endswith("_10min"):
                m = _inc_digit(m, 10, 6)
            elif field.endswith("_1min"):
                m = _inc_digit(m, 1, 10)
            if first:
                self._t1 = h * 60 + m
            else:
                self._t2 = h * 60 + m
            self._push_alarms()
            self.settingsChanged.emit()
            return
        if field == "update_sec":
            n = n.replace(second=0)
        elif field == "update_1min":
            n = n.replace(minute=_inc_digit(n.minute, 1, 10))
        elif field == "update_10min":
            n = n.replace(minute=_inc_digit(n.minute, 10, 6))
        elif field == "update_hr":
            n = n.replace(hour=(n.hour + 1) % 24)
        elif field == "update_mon":
            n = datetime.combine(_clamped(n.year, n.month % 12 + 1, n.day), n.time())
        elif field == "update_date":
            n = n.replace(day=n.day % calendar.monthrange(n.year, n.month)[1] + 1)
        elif field == "update_day":
            self._weekday_offset = (self._weekday_offset + 1) % 7
        elif field == "update_year":
            n = datetime.combine(_clamped(1980 if n.year >= 2099 else n.year + 1, n.month, n.day), n.time())
        elif field == "update_mode":
            self._24h = not self._24h
            self.settingsChanged.emit()
            return
        else:
            print(f"WatchModel: unknown field '{field}'", flush=True)
            return
        self._now = n
        self.timeChanged.emit()

    # --- stopwatch (simulated time) ------------------------------------------
    def _sw_start(self) -> None:
        if not self._sw_running:
            self._sw_running = True
            self._sw_start_ms = self._sim_ms
            self.stopwatchChanged.emit()

    def _sw_stop(self) -> None:
        if self._sw_running:
            self._sw_accum_ms += self._sim_ms - self._sw_start_ms
            self._sw_running = False
            self.stopwatchChanged.emit()

    def _sw_lap(self) -> None:
        self._lap = self.stopwatch_centis()
        self.stopwatchChanged.emit()

    def _sw_reset(self) -> None:
        self._sw_accum_ms = 0
        self._lap = 0
        self._sw_running = False
        self.stopwatchChanged.emit()
