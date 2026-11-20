"""TransitionTracer: active set, flashes and the event log (HOST-CONTRACT §3 `tracer`, §4)."""
from __future__ import annotations

import re

from PySide6.QtCore import QElapsedTimer, QObject, Property, Signal, Slot
from PySide6.QtScxml import QScxmlEvent, QScxmlStateMachine

from .scxml_events import connect_to_event, event_data, variant_text
from .scxml_tree import ScxmlTreeModel, Transition

LABEL_RE = re.compile(r"^T-\d+$")
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
MAX_ENTRIES = 500


class TransitionTracer(QObject):
    activeChanged = Signal()
    flashChanged = Signal()
    entriesChanged = Signal()

    def __init__(self, machine: QScxmlStateMachine, tree: ScxmlTreeModel, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._machine = machine
        self._tree = tree
        self._clock = QElapsedTimer()
        self._clock.start()
        self._active: set[str] = set()
        self._active_list: list[str] = []
        self._recent: list[set[str]] = []      # last 4 distinct stable configurations, newest first
        self._flash: dict[str, int] = {}
        self._entries: list[str] = []
        self._data: dict = {}                  # last host.data (the hv_* flags, D-24)
        # https://doc.qt.io/qt-6/qscxmlstatemachine.html#log
        machine.log.connect(self._on_log)
        # https://doc.qt.io/qt-6/qscxmlstatemachine.html#reachedStableState
        machine.reachedStableState.connect(self._on_stable)
        machine.runningChanged.connect(self._on_stable)
        for spec in ("model.*", "sw.*", "beep.*"):
            connect_to_event(machine, spec, self._on_outgoing, self)
        connect_to_event(machine, "host.data", self._on_host_data, self)
        self._on_stable()

    # --- properties -----------------------------------------------------------
    def _active_set(self) -> dict:
        return {s: True for s in self._active_list}

    def _flash_set(self) -> dict:
        return dict(self._flash)

    def _get_entries(self) -> list[str]:
        return self._entries

    activeSet = Property("QVariantMap", _active_set, notify=activeChanged)
    flashSet = Property("QVariantMap", _flash_set, notify=flashChanged)
    entries = Property("QStringList", _get_entries, notify=entriesChanged)

    # --- machine observers ----------------------------------------------------
    def _on_host_data(self, event: QScxmlEvent) -> None:
        self._data = event_data(event)

    def _on_outgoing(self, event: QScxmlEvent) -> None:
        data = event_data(event)
        self._append("◀ " + event.name() + "".join(f" {k}={variant_text(data[k])}" for k in sorted(data)))

    def _on_stable(self, *_: object) -> None:
        names = self._machine.activeStateNames(False)
        now = set(names)
        entered = now - self._active
        self._active = now
        self._active_list = list(names)
        # only distinct configurations (HOST-CONTRACT §4.3, DESIGN-NOTES D-26)
        if not self._recent or self._recent[0] != now:
            self._recent.insert(0, now)
        del self._recent[4:]
        self.activeChanged.emit()
        if entered:
            stamp = self._clock.elapsed()
            self._flash = {s: stamp for s in entered}
            self.flashChanged.emit()

    def _on_log(self, label: str, msg: str) -> None:
        if not LABEL_RE.match(label):
            return
        # The log signal names only the row and may arrive after the macrostep
        # (D-18): resolve source and targets from the parsed tree (§4.3).
        best = self._attribute(label)
        source, targets = (best.source, best.targets) if best else ("", [])
        stamp = self._clock.elapsed()
        for s in ([source] if source else []) + targets:
            self._flash[s] = stamp
        self.flashChanged.emit()
        to = ", ".join(targets) if targets else "(no state change)"
        self._append(f"{label} {msg or '·'}: {source} → {to}")

    def _attribute(self, label: str) -> Transition | None:
        nodes = self._tree.nodes

        def was_active(sid: str) -> bool:
            return any(sid in c for c in self._recent)

        def landed(t: Transition) -> bool:
            for tg in t.targets:
                n = nodes.get(tg)
                if (n.parent if n and n.kind == "history" else tg) not in self._active:
                    return False
            return True

        rows = [t for t in self._tree.transitions if t.specId == label and was_active(t.source)]
        cands = [t for t in rows if landed(t)] or rows
        if len(cands) <= 1:
            return cands[0] if cands else None
        # The clh(main*) pairs "[hv_x] -> x_H" / "-> x": hv_x from the last host.data.
        taken = uncond = None
        for t in cands:
            if not t.cond:
                uncond = t
            elif IDENT_RE.match(t.cond) and self._data.get(t.cond) is True:
                taken = t
        return taken or uncond or cands[0]

    # --- log ------------------------------------------------------------------
    def note_input(self, event: str) -> None:
        self._append(f"▶ {event}")

    def _append(self, line: str) -> None:
        self._entries.append(line)
        del self._entries[:-MAX_ENTRIES]
        self.entriesChanged.emit()

    @Slot()
    def clearLog(self) -> None:
        self._entries.clear()
        self.entriesChanged.emit()

    @Slot(str, result=bool)
    def exportLog(self, path: str) -> bool:
        if path.startswith("file://"):
            path = path[len("file://"):]
        text = "\n".join(self._entries) + "\n# active: " + " ".join(self._active_list) + "\n"
        try:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(text)
        except OSError:
            return False
        return True
