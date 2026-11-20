import QtQml
import QtScxml
import "../build/tracer.mjs" as Impl
import "../build/files.mjs" as Files
import "../build/ports.mjs" as Ports

// HOST-CONTRACT §3 `tracer` and §4 (src/tracer.mts).
QtObject {
    id: self
    required property var gate
    required property QtObject stateMachine
    required property var treeCore              // ScxmlTree (src/scxmlTree.mts)
    property string cwd: ""                     // relative export paths resolve here

    property var activeSet: ({})
    property var flashSet: ({})
    property var entries: []
    signal exported(string path, bool ok)

    readonly property var core: new Impl.TransitionTracer(() => self.gate, self.treeCore, () => Date.now(), {
        activeChanged: () => { self.activeSet = self.core.activeSet },
        flashChanged: () => { self.flashSet = self.core.flashSet },
        entriesChanged: () => { self.entries = self.core.entries },
    })

    function noteInput(event) { core.noteInput(event) }
    function clearLog() { core.clearLog() }
    function clearFlash() { core.clearFlash() }
    function logText() { return core.logText() }
    /**
     * §4.4. The file is written asynchronously (XMLHttpRequest PUT): true means the
     * write was started; `exported(path, ok)` and the optional `done(ok)` report it.
     */
    function exportLog(path, done) {
        return core.exportLog(String(path),
                              (p, text, finished) => Files.writeText(Files.fileUrl(p, self.cwd), text, finished),
                              (ok) => { self.exported(String(path), ok); if (done) done(ok) })
    }

    property EventConnection events: EventConnection {
        stateMachine: self.stateMachine
        events: ["model.*", "sw.*", "beep.*", "host.data"]
        onOccurred: (event) => self.core.onMachineEvent(event.name, Ports.eventData(event.data))
    }
    // https://doc.qt.io/qt-6/qscxmlstatemachine.html#log and #reachedStableState
    property Connections machineSignals: Connections {
        target: self.stateMachine
        function onLog(label, msg) { self.core.onLog(label, msg) }
        function onReachedStableState() { self.core.onStable() }
        function onRunningChanged() { self.core.onStable() }
    }
}
