import QtQml
import QtScxml
import "../build/watchModel.mjs" as Impl
import "../build/ports.mjs" as Ports

// HOST-CONTRACT §3 `watch`: QML face of src/watchModel.mts (the logic lives there).
QtObject {
    id: self
    required property var gate                  // MachineGate (src/machineGate.mts)
    required property QtObject stateMachine     // for EventConnection
    property bool running: false                // follow real time × speed

    property int hour: 12
    property int minute: 0
    property int second: 0
    property int year: 1987
    property int month: 6
    property int day: 1
    property int weekday: 1
    property bool is24h: false
    property int t1: 720
    property int t2: 720
    property real stopwatchCentis: 0
    property real lapCentis: 0
    property bool stopwatchRunning: false
    property real speed: 1
    signal shortBeep()

    readonly property var core: new Impl.WatchModel(() => self.gate, {
        timeChanged: () => self.syncTime(),
        settingsChanged: () => { self.is24h = self.core.is24h; self.t1 = self.core.t1; self.t2 = self.core.t2 },
        stopwatchChanged: () => {
            self.stopwatchCentis = self.core.stopwatchCentis
            self.lapCentis = self.core.lapCentis
            self.stopwatchRunning = self.core.stopwatchRunning
        },
        speedChanged: () => { if (self.speed !== self.core.speed) self.speed = self.core.speed },
        shortBeep: () => self.shortBeep(),
    })

    function syncTime() {
        hour = core.hour; minute = core.minute; second = core.second
        year = core.year; month = core.month; day = core.day; weekday = core.weekday
    }
    onSpeedChanged: {
        core.setSpeed(speed)
        if (speed !== core.speed) speed = core.speed
    }

    function setTime(h, m, s) { core.setTime(h, m, s) }
    function setAlarm(which, minutes) { core.setAlarm(which, minutes) }
    function jumpBeforeAlarm(which, secs) { core.jumpBeforeAlarm(which, secs) }
    function advance(ms) { core.advance(ms) }

    // model.*, sw.*, beep.*: https://doc.qt.io/qt-6/qml-qtscxml-eventconnection.html
    property EventConnection events: EventConnection {
        stateMachine: self.stateMachine
        events: ["model.reset", "model.increment", "sw.start", "sw.stop", "sw.lap", "sw.reset", "beep.short"]
        onOccurred: (event) => self.core.onMachineEvent(event.name, Ports.eventData(event.data))
    }
    // host.alarms once the machine can take it, as WatchModel does on initializedChanged
    property Connections init: Connections {
        target: self.stateMachine
        function onInitializedChanged() { self.core.pushAlarms() }
    }
    property Timer ticker: Timer {
        interval: 20; repeat: true; running: self.running; triggeredOnStart: true
        onTriggered: self.core.tick(Date.now())
    }
    Component.onCompleted: syncTime()
}
