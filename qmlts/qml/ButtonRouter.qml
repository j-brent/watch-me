import QtQml
import QtScxml
import "../build/buttonRouter.mjs" as Impl
import "../build/ports.mjs" as Ports

// HOST-CONTRACT §3 `buttons` (src/buttonRouter.mts).
QtObject {
    id: self
    required property var gate
    required property QtObject stateMachine
    required property QtObject tracer           // TransitionTracer.qml: logs every input

    property var held: ({})                     // button -> true; heldChanged comes with it
    signal dataValueChanged(string name)

    readonly property var core: new Impl.ButtonRouter(() => self.gate, {
        heldChanged: () => { self.held = self.core.held },
        dataValueChanged: (name) => self.dataValueChanged(name),
        noteInput: (event) => self.tracer.noteInput(event),
    })

    function press(b) { core.press(b) }
    function release(b) { core.release(b) }
    function releaseAll() { core.releaseAll() }
    function submit(event) { core.submit(String(event)) }
    function dataValue(name) { return core.dataValue(name) }
    function setDataValue(name, value) { core.setDataValue(name, value) }

    property EventConnection events: EventConnection {
        stateMachine: self.stateMachine
        events: ["host.data"]
        onOccurred: (event) => self.core.onMachineEvent(event.name, Ports.eventData(event.data))
    }
}
