// `tree` (docs/HOST-CONTRACT.md §3): watch.scxml parsed into a plain tree.
// The parsing is in scxml.mjs; this object only keeps the result and answers queries.
import QtQml
import "scxml.mjs" as Scxml
import "files.mjs" as Files

QtObject {
    id: model
    property var root: ({})
    property var stateIds: []
    property string errorString: ""
    property var _doc: ({ nodes: {}, transitions: [], order: [], rootId: "" })
    signal loaded()

    function load(url) {
        const xml = Files.readText(url)
        if (xml === null) {
            errorString = "cannot open " + url
            loaded()
            return false
        }
        return loadData(xml)
    }

    function loadData(xml) {
        const d = Scxml.parse(xml)
        _doc = d
        errorString = d.error
        stateIds = d.order
        root = d.rootId ? _toObject(d.rootId) : ({})
        loaded()
        return !d.error
    }

    // For TransitionTracer: the raw node table and transition list.
    function nodes() { return _doc.nodes }
    function transitions() { return _doc.transitions }
    function rootId() { return _doc.rootId }

    function _transition(t) {
        return { source: t.source, event: t.event, cond: t.cond, targets: t.targets.slice(),
                 type: t.type, specId: t.specId }
    }
    function _toObject(id) {
        const n = _doc.nodes[id]
        return {
            id: n.id, kind: n.kind, historyType: n.historyType, parent: n.parent,
            initial: n.initial, depth: n.depth,
            children: n.children.map(c => _toObject(c)),
            transitions: n.transitions.map(i => _transition(_doc.transitions[i])),
        }
    }

    function node(id) { return _doc.nodes[id] ? _toObject(id) : ({}) }
    function transitionsFrom(id) {
        const n = _doc.nodes[id]
        return n ? n.transitions.map(i => _transition(_doc.transitions[i])) : []
    }
    function transitionsInto(id) {
        return _doc.transitions.filter(t => t.targets.includes(id)).map(_transition)
    }
    function ancestors(id) {               // nearest first
        const out = []
        let p = _doc.nodes[id] ? _doc.nodes[id].parent : ""
        while (p) {
            out.push(p)
            p = _doc.nodes[p] ? _doc.nodes[p].parent : ""
        }
        return out
    }
    function specIds() {                   // sorted numerically, unique
        const ids = []
        for (const t of _doc.transitions)
            if (t.specId && !ids.includes(t.specId))
                ids.push(t.specId)
        return ids.sort((a, b) => a.localeCompare(b, undefined, { numeric: true }))
    }
}
