"""Observe the statechart's outgoing events from Python.

PySide6 6.8 only binds the string-based overload of
QScxmlStateMachine::connectToEvent(spec, receiver, SLOT(method)), not the functor
one (https://doc.qt.io/qt-6/qscxmlstatemachine.html#connectToEvent). So each
callback gets a small QObject with a real slot that takes a QScxmlEvent.
"""
from __future__ import annotations

from typing import Callable

from PySide6.QtCore import QObject, Slot, SLOT
from PySide6.QtScxml import QScxmlEvent, QScxmlStateMachine


class _EventSink(QObject):
    def __init__(self, callback: Callable[[QScxmlEvent], None], parent: QObject) -> None:
        super().__init__(parent)          # parented: lives as long as its owner
        self._callback = callback

    @Slot(QScxmlEvent)
    def deliver(self, event: QScxmlEvent) -> None:
        self._callback(event)


def connect_to_event(machine: QScxmlStateMachine, spec: str,
                     callback: Callable[[QScxmlEvent], None], owner: QObject) -> None:
    sink = _EventSink(callback, owner)
    if not machine.connectToEvent(spec, sink, SLOT("deliver(QScxmlEvent)")):
        raise RuntimeError(f"connectToEvent({spec!r}) failed")


def event_data(event: QScxmlEvent) -> dict:
    """The event's data as a dict. An event without <param>s has data "" (a QString)."""
    data = event.data()
    return data if isinstance(data, dict) else {}


def variant_text(value: object) -> str:
    """QVariant::toString() for the values the statechart sends (strings, numbers, bools)."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float) and value.is_integer():
        return str(int(value))
    return "" if value is None else str(value)
