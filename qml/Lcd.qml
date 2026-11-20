import QtQuick
import "LcdView.mjs" as LcdView

// The LCD. What it shows is a pure function of the active configuration
// (tracer.activeSet) and WatchModel's values — spec §4, in LcdView.mjs. No mode
// logic here.
Rectangle {
    id: lcd
    property var a: tracer.activeSet          // active state ids -> true
    function on(id) { return a[id] === true }

    // 2 Hz field blink (update), 1 Hz weak-battery blink, 2 Hz alarm flash
    property bool phase2: true
    property bool phase1: true
    Timer { interval: 250; running: true; repeat: true; onTriggered: lcd.phase2 = !lcd.phase2 }
    Timer { interval: 500; running: true; repeat: true; onTriggered: lcd.phase1 = !lcd.phase1 }

    readonly property bool dead: !on("alive")
    readonly property bool lit: on("light_on")
    readonly property bool beeping: on("alarms_beep") || on("chime_beep") || on("bt_beep_on") || shortBeep
    property bool shortBeep: false
    // audio follows the beeping states (optional, off by default)
    Binding { target: beeper; property: "continuous"; value: lcd.on("alarms_beep") || lcd.on("chime_beep") || lcd.on("bt_beep_on") }
    Connections {
        target: watch
        function onShortBeep() { lcd.shortBeep = true; shortBeepTimer.restart() }
    }
    Timer { id: shortBeepTimer; interval: 500; onTriggered: lcd.shortBeep = false }

    // whole-LCD visibility: weak battery blinks at 1 Hz, alarms-beep flashes at 2 Hz
    readonly property bool contentVisible: !dead && !(on("power_blink") && !phase1)
                                           && !(on("alarms_beep") && !phase2)

    color: lit ? "#9fe6d6" : "#b9c4a6"
    border.color: "#3a3f36"
    border.width: 3
    radius: 10
    implicitWidth: 330
    implicitHeight: 190

    Behavior on color { ColorAnimation { duration: 120 } }

    // The display, derived from the configuration (spec §4): LcdView.mjs, tested
    // by tests/tst_lcd.cpp. Reading tracer.activeSet and watch.* inside view()
    // makes them dependencies of this binding.
    readonly property var view: LcdView.view(lcd.on, watch, !lcd.phase2)

    readonly property color ink: "#1b1f1a"
    readonly property color ghost: lit ? "#1b1f1a14" : "#1b1f1a12"

    Item {
        anchors.fill: parent
        anchors.margins: 12
        visible: lcd.contentVisible

        // top row: four indicator windows (chime, alarm 1, alarm 2, stopwatch), cf. Fig. 7
        Row {
            id: icons
            spacing: 8
            Repeater {
                model: [
                    { txt: "CHIME", show: lcd.on("chime_enabled") },
                    { txt: "AL 1", show: lcd.on("alarm1_enabled") },
                    { txt: "AL 2", show: lcd.on("alarm2_enabled") },
                    { txt: "STW", show: watch.stopwatchRunning }
                ]
                delegate: Rectangle {
                    width: 70; height: 30; radius: 4
                    color: "transparent"
                    border.color: "#3a3f3655"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.txt
                        visible: modelData.show
                        color: lcd.ink
                        font.pixelSize: 16; font.bold: true
                    }
                }
            }
        }

        Text {
            id: label
            anchors.top: icons.bottom; anchors.topMargin: 10
            text: lcd.view.label
            visible: !lcd.view.blinkLabel
            color: lcd.ink
            font.pixelSize: 20; font.bold: true; font.family: "monospace"
        }
        Text {
            anchors.top: label.bottom; anchors.topMargin: 2
            text: lcd.view.status
            color: lcd.ink
            font.pixelSize: 14; font.bold: true
        }
        Text {
            anchors.top: icons.bottom; anchors.topMargin: 10
            anchors.right: parent.right
            text: lcd.view.lap ? "LAP" : ""
            color: lcd.ink
            font.pixelSize: 14; font.bold: true
        }

        Row {
            id: digits
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            spacing: 6
            LcdDigits {
                text: lcd.view.main.substr(0, 2)
                blank: lcd.view.blinkMain.filter(i => i < 2)
                digitWidth: 38; digitHeight: 70
                onColor: lcd.ink; offColor: lcd.ghost
            }
            Item {
                width: 14; height: 70
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    visible: lcd.view.sep === ":"
                    Rectangle { width: 7; height: 7; color: lcd.ink }
                    Rectangle { width: 7; height: 7; color: lcd.ink }
                }
                Rectangle {
                    anchors.centerIn: parent
                    visible: lcd.view.sep === "-"
                    width: 12; height: 6; color: lcd.ink
                }
            }
            LcdDigits {
                text: lcd.view.main.substr(2, 2)
                blank: lcd.view.blinkMain.filter(i => i >= 2).map(i => i - 2)
                digitWidth: 38; digitHeight: 70
                onColor: lcd.ink; offColor: lcd.ghost
            }
            Column {
                anchors.bottom: parent.bottom
                spacing: 4
                Text {
                    text: lcd.view.ampm
                    visible: !lcd.view.ampmBlink
                    color: lcd.ink
                    font.pixelSize: 13; font.bold: true
                }
                LcdDigits {
                    text: lcd.view.small
                    blank: lcd.view.blinkSmall ? [0, 1] : []
                    digitWidth: 20; digitHeight: 36
                    onColor: lcd.ink; offColor: lcd.ghost
                }
            }
        }
    }

    // beeper indicator (visual stand-in for the two-tone beeper)
    Rectangle {
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: -10
        width: 40; height: 40; radius: 20
        color: lcd.beeping ? "#e0542f" : "transparent"
        border.color: lcd.beeping ? "#7a2510" : "transparent"
        Text { anchors.centerIn: parent; text: "BEEP"; color: "white"; font.pixelSize: 9; font.bold: true; visible: lcd.beeping }
        SequentialAnimation on scale {
            running: lcd.beeping; loops: Animation.Infinite
            NumberAnimation { to: 1.2; duration: 150 }
            NumberAnimation { to: 1.0; duration: 150 }
        }
    }
}
