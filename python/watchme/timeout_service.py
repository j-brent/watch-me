"""TimeoutService: the clock behind the statechart's "n sec in X" timers
(HOST-CONTRACT §5.2, DESIGN-NOTES D-20/D-21). The statechart decides every timer;
this only counts time and submits timeout.<id>{gen} when a deadline passes."""
from __future__ import annotations

from PySide6.QtCore import QCoreApplication, QElapsedTimer, QEvent, QObject, QTimer
from PySide6.QtScxml import QScxmlStateMachine

from .scxml_events import connect_to_event, event_data


def drain_machine() -> None:
    """Let the machine process what has been submitted to it, now. Qt SCXML runs its
    external queue from a queued meta-call, so deliver posted MetaCall events.
    https://doc.qt.io/qt-6/qcoreapplication.html#sendPostedEvents"""
    QCoreApplication.sendPostedEvents(None, QEvent.Type.MetaCall)


class TimeoutService(QObject):
    def __init__(self, machine: QScxmlStateMachine, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._machine = machine
        self._pending: dict[str, tuple[int, object]] = {}    # id -> (due ms, gen)
        self._now = 0
        self._advancing = False
        self._last_real = 0
        self._real = QElapsedTimer()
        self._timer = QTimer(self)
        self._timer.setInterval(20)
        self._timer.timeout.connect(self._on_timer)
        connect_to_event(machine, "timer.start", self._on_start, self)
        connect_to_event(machine, "timer.cancel", self._on_cancel, self)

    def _on_start(self, event) -> None:
        d = event_data(event)
        # round() is banker's rounding; qRound64 rounds half away from zero.
        ms = int(float(d.get("secs", 0)) * 1000.0 + 0.5)
        self._pending[str(d.get("id", ""))] = (self._now + ms, d.get("gen"))

    def _on_cancel(self, event) -> None:
        self._pending.pop(str(event_data(event).get("id", "")), None)

    def _on_timer(self) -> None:
        r = self._real.elapsed()
        self.advance(r - self._last_real)
        self._last_real = r

    def set_running(self, running: bool) -> None:
        if running == self._timer.isActive():
            return
        if running:
            self._real.start()
            self._last_real = 0
            self._timer.start()
        else:
            self._timer.stop()

    def advance(self, ms: int) -> None:
        if ms < 0 or self._advancing:
            return
        self._advancing = True
        try:
            # Timers just requested (e.g. by a button) start at the current time.
            drain_machine()
            end = self._now + ms
            while True:
                due_ids = [(due, tid) for tid, (due, _) in self._pending.items() if due <= end]
                if not due_ids:
                    break
                due, tid = min(due_ids)              # earliest; ties: smaller id
                self._now = due
                _, gen = self._pending.pop(tid)
                self._machine.submitEvent(f"timeout.{tid}", {"gen": gen})
                # Handle it at its own deadline: a timer it starts counts from `due`
                # and may fire within this same step (D-21).
                drain_machine()
            self._now = end
        finally:
            self._advancing = False
