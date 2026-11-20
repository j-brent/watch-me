"""SpecTags (confidence badges from STATECHART-SPEC.md) and Beeper (optional sound)."""
from __future__ import annotations

import importlib.util
import math
import re
import struct
import tempfile
from pathlib import Path

from PySide6.QtCore import QObject, Property, QUrl, Signal, Slot

ROW_RE = re.compile(r"^\|\s*(T-\d+)\s*\|.*\[(P|F|A)\]", re.MULTILINE)
RANK = {"A": 0, "F": 1, "P": 2}


class SpecTags(QObject):
    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._tags: dict[str, str] = {}

    def load(self, path: Path) -> None:
        for m in ROW_RE.finditer(path.read_text(encoding="utf-8")):
            self._tags[m.group(1)] = m.group(2)

    @Slot(str, result=str)
    def tagFor(self, spec_id: str) -> str:
        return self._tags.get(spec_id, "")

    @Slot("QStringList", result=str)
    def weakest(self, spec_ids: list[str]) -> str:
        known = [self._tags[i] for i in spec_ids if i in self._tags]
        return min(known, key=RANK.__getitem__, default="")


def _wav(seconds: float, two_tone: bool) -> bytes:
    rate = 22050
    samples = bytearray()
    for i in range(int(seconds * rate)):
        t = i / rate
        f = 2730.0 if two_tone and int(t * 8) % 2 else 2048.0      # piezo-like square wave
        samples += struct.pack("<h", 9000 if math.sin(2 * math.pi * f * t) > 0 else -9000)
    header = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(samples), b"WAVE", b"fmt ", 16, 1, 1,
                         rate, rate * 2, 2, 16, b"data", len(samples))
    return header + bytes(samples)


class Beeper(QObject):
    enabledChanged = Signal()
    continuousChanged = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._enabled = False
        self._continuous = False
        self._fx = None                    # (tempdir, short, loop), created on first enable
        # Loaded on first enable only; a wheel without QtMultimedia: no sound.
        self._available = importlib.util.find_spec("PySide6.QtMultimedia") is not None

    def _get_enabled(self) -> bool: return self._enabled
    def _get_continuous(self) -> bool: return self._continuous
    def _get_available(self) -> bool: return self._available

    def _set_enabled(self, e: bool) -> None:
        if e != self._enabled:
            self._enabled = e
            self._update()
            self.enabledChanged.emit()

    def _set_continuous(self, c: bool) -> None:
        if c != self._continuous:
            self._continuous = c
            self._update()
            self.continuousChanged.emit()

    enabled = Property(bool, _get_enabled, _set_enabled, notify=enabledChanged)
    continuous = Property(bool, _get_continuous, _set_continuous, notify=continuousChanged)
    available = Property(bool, _get_available, constant=True)

    @Slot()
    def shortBeep(self) -> None:           # T-56
        if self._enabled and self._fx:
            self._fx[1].play()

    def _update(self) -> None:
        if not self._available:
            return
        if self._enabled and not self._fx:
            from PySide6.QtMultimedia import QSoundEffect
            tmp = tempfile.TemporaryDirectory()
            effects = []
            for name, secs, two in (("short.wav", 0.5, False), ("loop.wav", 1.0, True)):
                p = Path(tmp.name, name)
                p.write_bytes(_wav(secs, two))
                fx = QSoundEffect(self)
                fx.setSource(QUrl.fromLocalFile(str(p)))
                effects.append(fx)
            effects[1].setLoopCount(QSoundEffect.Loop.Infinite.value)
            self._fx = (tmp, *effects)
        if not self._fx:
            return
        loop = self._fx[2]
        if self._enabled and self._continuous:
            if not loop.isPlaying():
                loop.play()
        else:
            loop.stop()
