// The clock behind the statechart's "n sec in X" timers (docs/HOST-CONTRACT.md §5.2,
// DESIGN-NOTES D-20, D-21). watch.scxml sends timer.start{id, secs, gen} and
// timer.cancel{id}; this object counts time and, when a deadline passes, submits
// timeout.<id>{gen}. Port of cpp/src/TimeoutService.cpp, with one difference:
//
// The C++ host lets the machine process what was submitted *synchronously*
// (QCoreApplication::sendPostedEvents). JavaScript cannot drain the event queue,
// and QScxmlStateMachine processes submitted events from a queued call. So
// advance() is asynchronous: it waits until the machine has settled (see
// _settle), fires the earliest deadline, waits again, and so on. While one
// advance() is running, further advance() calls are added to a backlog and run
// after it (the C++ host never sees that case: its advance() cannot be interrupted).
import QtQml
import QtScxml

QtObject {
    id: service
    property QtObject machine
    property real now: 0                 // ms
    property var pending: ({})           // id -> {due, gen}
    property bool running: false         // follow real time at 1x (not --manual-time)
    readonly property bool busy: _busy
    property bool _busy: false
    property real _backlog: 0
    property int _activity: 0            // machine signals seen; changes while it works

    signal fired(string id)
    signal settled()                     // an advance() and its backlog are done

    // Pending deadline of a timer in ms from now, or -1 if it is not armed.
    function remaining(id) { return pending[id] ? pending[id].due - now : -1 }

    function advance(ms) {
        if (!(ms >= 0))
            return
        if (_busy) {                     // does not re-enter: run after the current step
            _backlog += ms
            return
        }
        _busy = true
        // 1. Timers already requested (e.g. by a button just submitted) start at the
        //    current time, not at the end of this step.
        _settle(() => _step(now + ms))
    }

    function _step(end) {
        // 2. earliest pending deadline within this step (ties: smaller id)
        let id = "", due = Infinity
        for (const k of Object.keys(pending)) {
            const p = pending[k]
            if (p.due <= end && (p.due < due || (p.due === due && k < id))) {
                due = p.due
                id = k
            }
        }
        if (!id) {
            // 3. the end of the step
            now = end
            _busy = false
            if (_backlog > 0) {
                const b = _backlog
                _backlog = 0
                advance(b)
            } else {
                settled()
            }
            return
        }
        now = due
        const p = pending[id]
        const rest = Object.assign({}, pending)
        delete rest[id]
        pending = rest
        fired(id)
        machine.submitEvent("timeout." + id, { gen: p.gen })
        // Handle the timeout at its own time `due`: a timer it starts is due
        // relative to `due` and may itself fire within this step (D-21).
        _settle(() => _step(end))
    }

    // Calls `done` once the machine has processed everything submitted so far.
    // QScxmlStateMachine handles its external queue in a queued meta-call;
    // Qt.callLater also runs from the event queue, after what was posted before it
    // (https://doc.qt.io/qt-6/qml-qtqml-qt.html#callLater-method). The machine may
    // post more work while it runs (its own <send>s), so wait until two turns in a
    // row pass without any sign of activity (reachedStableState, log, an event).
    function _settle(done) {
        let quiet = 0
        let seen = _activity
        const tick = () => {
            if (_activity !== seen) {
                seen = _activity
                quiet = 0
            } else {
                ++quiet
            }
            if (quiet >= 2)
                done()
            else
                Qt.callLater(tick)
        }
        Qt.callLater(tick)
    }

    property Connections _machineSignals: Connections {
        target: service.machine
        function onReachedStableState() { ++service._activity }
        function onLog() { ++service._activity }
    }
    // https://doc.qt.io/qt-6/qml-qtscxml-eventconnection.html
    property EventConnection _timers: EventConnection {
        stateMachine: service.machine
        events: ["timer.start", "timer.cancel"]
        onOccurred: (event) => {
            ++service._activity
            const d = event.data || {}
            const p = Object.assign({}, service.pending)
            if (event.name === "timer.start")
                p[String(d.id)] = { due: service.now + Math.round(Number(d.secs) * 1000), gen: d.gen }
            else
                delete p[String(d.id)]
            service.pending = p
        }
    }

    // Real time at 1x, about every 20 ms.
    property real _lastReal: 0
    property Timer _timer: Timer {
        interval: 20
        repeat: true
        running: service.running
        onRunningChanged: if (running) service._lastReal = Date.now()
        onTriggered: {
            const r = Date.now()
            service.advance(r - service._lastReal)
            service._lastReal = r
        }
    }
}
