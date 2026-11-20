"""ButtonRouter: watch buttons and other input as SCXML events (HOST-CONTRACT §3 `buttons`).
Tracks what is held, so key auto-repeat never sends a second .down."""
from __future__ import annotations

from PySide6.QtCore import QObject, Property, Signal, Slot
from PySide6.QtScxml import QScxmlEvent, QScxmlStateMachine

from .scxml_events import connect_to_event, event_data
from .tracer import TransitionTracer

BUTTONS = ("a", "b", "c", "d")


class ButtonRouter(QObject):
    heldChanged = Signal()
    dataValueChanged = Signal(str)   # "" for the first host.data (everything is new)

    def __init__(self, machine: QScxmlStateMachine, tracer: TransitionTracer, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._machine = machine
        self._tracer = tracer
        self._held: list[str] = []
        self._data: dict = {}
        connect_to_event(machine, "host.data", self._on_host_data, self)

    def _on_host_data(self, event: QScxmlEvent) -> None:
        before, self._data = self._data, event_data(event)
        if not before:
            self.dataValueChanged.emit("")
            return
        for key, value in self._data.items():
            if before.get(key) != value:
                self.dataValueChanged.emit(key)

    def _get_held(self) -> dict:
        return {b: True for b in self._held}

    held = Property("QVariantMap", _get_held, notify=heldChanged)

    @Slot(str)
    def press(self, button: str) -> None:
        b = button.lower()
        if b not in BUTTONS or b in self._held:
            return
        self._held.append(b)
        self.heldChanged.emit()
        self.submit(f"{b}.down")

    @Slot(str)
    def release(self, button: str) -> None:
        b = button.lower()
        if b not in self._held:
            return
        self._held.remove(b)
        self.heldChanged.emit()
        self.submit(f"{b}.up")

    @Slot()
    def releaseAll(self) -> None:
        for b in list(self._held):
            self.release(b)

    @Slot(str)
    def submit(self, event: str) -> None:
        self._tracer.note_input(event)
        self._machine.submitEvent(event)

    @Slot(str, result="QVariant")
    def dataValue(self, name: str) -> object:
        return self._data.get(name)

    @Slot(str, "QVariant")
    def setDataValue(self, name: str, value: object) -> None:
        # The statechart answers with host.data, which updates dataValue() (D-24).
        if name == "harelAnomalies":
            self._machine.submitEvent("host.anomalies", {"value": bool(value)})
        else:
            print(f"ButtonRouter: '{name}' cannot be set (DESIGN-NOTES D-24)", flush=True)
