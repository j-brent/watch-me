// `tracer` (docs/HOST-CONTRACT.md §3, §4): the active configuration, the flash
// stamps and the event log. Port of cpp/src/TransitionTracer.cpp.
import QtQml
import QtScxml
import "files.mjs" as Files

QtObject {
    id: tracer
    property QtObject machine            // QScxmlStateMachine
    property QtObject tree               // ScxmlTreeModel
    property string cwd: ""              // for relative export paths (the launcher passes it)
    property int maxEntries: 500

    property var activeStates: []        // activeStateNames(false)
    property var activeSet: ({})         // stateId -> true
    property var flashSet: ({})          // stateId -> stamp
    property var entries: []
    property string lastSpecId: ""
    property var _fired: ({})
    property var _recent: []             // last 4 distinct stable configurations (sets), newest first
    property var _prevActive: ({})
    property var _data: ({})             // last host.data: the hv_* flags (D-24)
    property real _stamp: 0

    signal transitionFired(string specId, string event, string source, var targets)

    function firedSpecIds() {
        return Object.keys(_fired).sort((a, b) => a.localeCompare(b, undefined, { numeric: true }))
    }
    function isActive(id) { return activeSet[id] === true }
    function noteInput(event) { _append("▶ " + event) }
    function clearLog() { entries = [] }
    function logText() { return entries.join("\n") }
    function clearFlash() { flashSet = ({}) }

    // §4.4. QML can only write files asynchronously (files.mjs), so the return
    // value says that the write was started; `exported(path, ok)` (and a warning
    // when it failed) tells how it ended. The host waits for it before quitting.
    signal exported(string path, bool ok)
    function exportLog(fileName) {
        let path = String(fileName)
        if (path.startsWith("file://"))
            path = path.slice(7)
        if (!path)
            return false
        const text = logText() + "\n# active: " + activeStates.join(" ") + "\n"
        return Files.writeText(Files.toUrl(path, cwd), text, ok => {
            if (!ok)
                console.warn("could not export the log to " + path)
            tracer.exported(path, ok)
        })
    }

    function _nextStamp() {
        // any value that changes when a state flashes again: ms, strictly increasing
        _stamp = Math.max(Date.now(), _stamp + 1)
        return _stamp
    }

    function _append(line) {
        const e = entries.slice()
        e.push(line)
        while (e.length > maxEntries)
            e.shift()
        entries = e
    }

    function _onStable() {
        if (!machine)
            return
        const names = machine.activeStateNames(false)
        const now = {}
        for (const s of names)
            now[s] = true
        // States newly entered in this macrostep flash as well.
        const pending = names.filter(s => !_prevActive[s])
        activeStates = names
        activeSet = now
        _prevActive = now
        // only distinct configurations (HOST-CONTRACT §4.3, DESIGN-NOTES D-26)
        const r = _recent.slice()
        const newest = r[0]
        const same = newest !== undefined && Object.keys(newest).length === Object.keys(now).length
                     && Object.keys(now).every(s => newest[s] === true)
        if (!same)
            r.unshift(now)
        while (r.length > 4)
            r.pop()
        _recent = r
        if (pending.length) {
            const f = {}
            const stamp = _nextStamp()
            for (const s of pending)
                f[s] = stamp
            flashSet = f
        }
    }

    // §4.3: the log signal only names the row (and Qt delivers it after the
    // macrostep, D-18), so the source and targets come from the parsed tree.
    function _onLog(label, msg) {
        if (!/^T-\d+$/.test(label))
            return
        let source = "", targets = []
        if (tree) {
            const nodes = tree.nodes()
            const all = tree.transitions()
            const wasActive = id => _recent.some(c => c[id] === true)
            const landed = t => t.targets.every(tg => {
                const n = nodes[tg]
                const where = n && n.kind === "history" ? n.parent : tg
                return activeSet[where] === true
            })
            let cands = all.filter(t => t.specId === label && wasActive(t.source) && landed(t))
            if (!cands.length)
                cands = all.filter(t => t.specId === label && wasActive(t.source))
            let best = cands.length ? cands[0] : null
            if (cands.length > 1) {
                // Same row, same source: the clh(main*) pairs "[hv_x] -> x_H" / "-> x".
                let uncond = null, taken = null
                for (const t of cands) {
                    if (!t.cond)
                        uncond = t
                    else if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(t.cond) && !!_data[t.cond])
                        taken = t
                }
                best = taken || uncond || best
            }
            if (best) {
                source = best.source
                targets = best.targets.slice()
            }
        }
        const fired = Object.assign({}, _fired)
        fired[label] = true
        _fired = fired
        lastSpecId = label
        const stamp = _nextStamp()
        const f = Object.assign({}, flashSet)
        if (source)
            f[source] = stamp
        for (const t of targets)
            f[t] = stamp
        flashSet = f
        const to = targets.length ? targets.join(", ") : "(no state change)"
        _append(label + " " + (msg ? msg : "·") + ": " + source + " → " + to)
        transitionFired(label, msg, source, targets)
    }

    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#log
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#reachedStableState
    property Connections _machineSignals: Connections {
        target: tracer.machine
        function onLog(label, msg) { tracer._onLog(label, msg) }
        function onReachedStableState() { tracer._onStable() }
        function onRunningChanged() { tracer._onStable() }
    }
    // Outgoing events (D-11): "◀ name key=value …", keys in order.
    property EventConnection _outgoing: EventConnection {
        stateMachine: tracer.machine
        events: ["model.*", "sw.*", "beep.*"]
        onOccurred: (event) => {
            let line = "◀ " + event.name
            const d = event.data
            if (d && typeof d === "object")
                for (const k of Object.keys(d).sort())
                    line += " " + k + "=" + String(d[k])
            tracer._append(line)
        }
    }
    property EventConnection _hostData: EventConnection {
        stateMachine: tracer.machine
        events: ["host.data"]
        onOccurred: (event) => { tracer._data = Object.assign({}, event.data) }
    }
    onMachineChanged: _onStable()
}
