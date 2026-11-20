"""ScxmlTreeModel: watch.scxml parsed with an XML reader (not Qt SCXML) into a plain
tree for the statechart view (HOST-CONTRACT §3 `tree`)."""
from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

from PySide6.QtCore import Property, QObject, Signal, Slot

NS = "{http://www.w3.org/2005/07/scxml}"
STATE_TAGS = {"state", "parallel", "final", "history"}
SPEC_RE = re.compile(r"\bT-\d+\b")


@dataclass
class Transition:
    source: str
    event: str
    cond: str
    targets: list[str]
    type: str
    specId: str = ""

    def as_map(self) -> dict:
        return {"source": self.source, "event": self.event, "cond": self.cond,
                "targets": list(self.targets), "type": self.type, "specId": self.specId}


@dataclass
class Node:
    id: str
    kind: str                     # root | compound | parallel | atomic | history | final
    parent: str = ""
    depth: int = 0
    historyType: str = ""
    initial: str = ""
    children: list[str] = field(default_factory=list)
    transitions: list[int] = field(default_factory=list)


class ScxmlTreeModel(QObject):
    loaded = Signal()

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self.nodes: dict[str, Node] = {}
        self.transitions: list[Transition] = []
        self.root_id = ""
        self._root_map: dict = {}

    def load(self, path: str) -> None:
        # insert_comments: the "<!-- T-xx -->" fallback for transitions without a <log>
        parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
        root = ET.parse(path, parser).getroot()
        anon = 0
        last: Transition | None = None      # the last transition read, in document order

        def local(tag) -> str:
            return tag[len(NS):] if isinstance(tag, str) and tag.startswith(NS) else str(tag)

        def add(node_id: str, kind: str, parent: str) -> Node:
            n = Node(node_id, kind, parent, self.nodes[parent].depth + 1 if parent else 0)
            if parent:
                self.nodes[parent].children.append(node_id)
            self.nodes[node_id] = n
            return n

        def walk(el: ET.Element, owner: str) -> None:
            nonlocal anon, last
            for child in el:
                if child.tag is ET.Comment:
                    if last is not None and not last.specId and (m := SPEC_RE.search(child.text or "")):
                        last.specId = m.group(0)
                    continue
                tag = local(child.tag)
                if tag in STATE_TAGS:
                    sid = child.get("id") or f"_anon{(anon := anon + 1)}"
                    n = add(sid, "atomic" if tag == "state" else tag, owner)
                    if tag == "history":
                        n.historyType = child.get("type") or "shallow"
                    n.initial = child.get("initial", "")
                    walk(child, sid)
                elif tag == "transition":
                    # Not chart transitions: a <history>'s default (D-22), host.* (D-24).
                    event = child.get("event", "")
                    if self.nodes[owner].kind == "history" or event.startswith("host."):
                        continue
                    t = Transition(owner, event, child.get("cond", ""), (child.get("target") or "").split(),
                                   child.get("type", "external"))
                    self.transitions.append(t)
                    self.nodes[owner].transitions.append(len(self.transitions) - 1)
                    last = t
                    for sub in child.iter(f"{NS}log"):
                        if not t.specId and (m := SPEC_RE.search(sub.get("label", ""))):
                            t.specId = m.group(0)
                    for sub in child.iter():          # comments inside the transition
                        if sub.tag is ET.Comment and not t.specId and (m := SPEC_RE.search(sub.text or "")):
                            t.specId = m.group(0)

        self.root_id = root.get("name", "scxml")
        add(self.root_id, "root", "").initial = root.get("initial", "")
        walk(root, self.root_id)

        # Classify states and resolve implicit initial children (first child state).
        for n in self.nodes.values():
            if n.kind == "atomic" and any(self.nodes[c].kind != "history" for c in n.children):
                n.kind = "compound"
            if n.kind in ("compound", "root") and not n.initial:
                n.initial = next((c for c in n.children if self.nodes[c].kind != "history"), "")
        self._root_map = self._to_map(self.root_id)
        self.loaded.emit()

    def _to_map(self, node_id: str) -> dict:
        n = self.nodes[node_id]
        return {"id": n.id, "kind": n.kind, "historyType": n.historyType, "parent": n.parent,
                "initial": n.initial, "depth": n.depth,
                "children": [self._to_map(c) for c in n.children],
                "transitions": [self.transitions[i].as_map() for i in n.transitions]}

    def _root(self) -> dict:
        return self._root_map

    root = Property("QVariantMap", _root, notify=loaded)

    @Slot(str, result="QVariantList")
    def transitionsFrom(self, node_id: str) -> list:
        n = self.nodes.get(node_id)
        return [self.transitions[i].as_map() for i in n.transitions] if n else []

    @Slot(str, result="QVariantList")
    def transitionsInto(self, node_id: str) -> list:
        return [t.as_map() for t in self.transitions if node_id in t.targets]

    @Slot(str, result="QStringList")
    def ancestors(self, node_id: str) -> list[str]:
        out = []
        p = self.nodes[node_id].parent if node_id in self.nodes else ""
        while p:
            out.append(p)
            p = self.nodes[p].parent
        return out
