import QtQml
import QtScxml
import "../build/timeoutService.mjs" as Impl
import "../build/ports.mjs" as Ports

// HOST-CONTRACT §5.2: the clock behind "n sec in X" (src/timeoutService.mts).
QtObject {
    id: self
    required property var gate                  // MachineGate: submit + settle
    required property QtObject stateMachine
    property bool running: false                // follow real time at 1×

    readonly property var core: new Impl.TimeoutService(() => self.gate, (fn) => self.gate.settle(fn))

    /** Asynchronous: `done` (optional) runs when every deadline in the step is handled. */
    function advance(ms, done) { core.advance(ms, done) }
    function remaining(id) { return core.remaining(id) }

    property EventConnection events: EventConnection {
        stateMachine: self.stateMachine
        events: ["timer.start", "timer.cancel"]
        onOccurred: (event) => self.core.onMachineEvent(event.name, Ports.eventData(event.data))
    }
    property Timer ticker: Timer {
        interval: 20; repeat: true; running: self.running; triggeredOnStart: true
        onTriggered: self.core.tick(Date.now())
    }
}
