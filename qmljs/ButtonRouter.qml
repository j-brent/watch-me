// `buttons` (docs/HOST-CONTRACT.md §3): the four watch buttons and every other
// input event. Keeps track of what is held, so key auto-repeat and mouse+keyboard
// combinations never send a second ".down" without an ".up".
// Port of cpp/src/ButtonRouter.cpp.
import QtQml
import QtScxml

QtObject {
    id: router
    property QtObject machine            // QScxmlStateMachine (StateMachineLoader.stateMachine)
    property QtObject tracer             // TransitionTracer, for the "▶ event" log lines
    property var held: ({})              // button -> true; heldChanged is the property's own signal
    property var _data: null             // last host.data (DESIGN-NOTES D-24)

    // host.data changed `name`; "": the first host.data (every value is new).
    signal dataValueChanged(string name)

    function _valid(b) { return b === "a" || b === "b" || b === "c" || b === "d" }

    function press(button) {
        const b = String(button).toLowerCase()
        if (!_valid(b) || held[b])
            return
        const h = Object.assign({}, held)
        h[b] = true
        held = h
        submit(b + ".down")
    }
    function release(button) {
        const b = String(button).toLowerCase()
        if (!held[b])
            return
        const h = Object.assign({}, held)
        delete h[b]
        held = h
        submit(b + ".up")
    }
    function releaseAll() {
        for (const b of Object.keys(held))
            release(b)
    }
    function submit(event) {
        if (tracer)
            tracer.noteInput(event)
        // https://doc.qt.io/qt-6/qscxmlstatemachine.html#submitEvent-1
        machine.submitEvent(event)
    }
    // Sim panel and `set:` steps, through the host boundary (D-24). Nothing changes
    // here until the statechart answers with host.data.
    function setDataValue(name, value) {
        if (name === "harelAnomalies")
            machine.submitEvent("host.anomalies", { value: !!value })
        else
            console.warn("ButtonRouter: '" + name + "' cannot be set (DESIGN-NOTES D-24)")
    }
    function dataValue(name) { return _data ? _data[name] : undefined }

    // https://doc.qt.io/qt-6/qml-qtscxml-eventconnection.html
    property EventConnection _hostData: EventConnection {
        stateMachine: router.machine
        events: ["host.data"]
        onOccurred: (event) => {
            const now = Object.assign({}, event.data)
            const before = router._data
            router._data = now
            if (!before || Object.keys(before).length === 0) {
                router.dataValueChanged("")
                return
            }
            for (const k of Object.keys(now))
                if (before[k] !== now[k])
                    router.dataValueChanged(k)
        }
    }
}
