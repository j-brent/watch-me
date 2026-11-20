import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Simulation controls. Everything here is an *input* to the machine or to the
// clock model; none of it decides behaviour.
Rectangle {
    id: panel
    color: "#fdfcf9"; border.color: "#d4d0c6"; radius: 6
    implicitHeight: col.implicitHeight + 16

    function two(n) { return (n < 10 ? "0" : "") + n }

    // Emitted when a text/number field is done with: Main gives keyboard focus back
    // to the A/B/C/D keys.
    signal editingDone()

    ColumnLayout {
        id: col
        x: 8; y: 8
        width: panel.width - 16
        spacing: 6

        // ---- battery (Fig. 26/28 events)
        RowLayout {
            spacing: 4
            Label { text: "Battery"; font.bold: true; Layout.preferredWidth: 62 }
            Repeater {
                model: [["inserted", "battery.inserted"], ["weakens", "battery.weakens"],
                        ["dies", "battery.dies"], ["removed", "battery.removed"]]
                delegate: Button {
                    text: modelData[0]
                    focusPolicy: Qt.NoFocus
                    onClicked: buttons.submit(modelData[1])
                }
            }
        }

        // ---- clock speed (log scale 1x .. 600x)
        RowLayout {
            spacing: 6
            Label { text: "Speed"; font.bold: true; Layout.preferredWidth: 62 }
            Slider {
                id: speed
                Layout.fillWidth: true
                from: 0; to: Math.log(600) / Math.LN10
                value: Math.log(Math.max(1, watch.speed)) / Math.LN10
                focusPolicy: Qt.NoFocus
                onMoved: watch.speed = Math.round(Math.pow(10, value))
            }
            Label { text: Math.round(watch.speed) + "×"; Layout.preferredWidth: 44 }
        }

        // ---- set T
        RowLayout {
            spacing: 4
            Label { text: "Set T"; font.bold: true; Layout.preferredWidth: 62 }
            SpinBox { id: hh; Keys.onReturnPressed: panel.editingDone(); Keys.onEnterPressed: panel.editingDone(); Keys.onEscapePressed: panel.editingDone(); from: 0; to: 23; value: 12; editable: true; Layout.preferredWidth: 92; focusPolicy: Qt.ClickFocus }
            Label { text: ":" }
            SpinBox { id: mm; Keys.onReturnPressed: panel.editingDone(); Keys.onEnterPressed: panel.editingDone(); Keys.onEscapePressed: panel.editingDone(); from: 0; to: 59; value: 0; editable: true; Layout.preferredWidth: 92; focusPolicy: Qt.ClickFocus }
            Label { text: ":" }
            SpinBox { id: ss; Keys.onReturnPressed: panel.editingDone(); Keys.onEnterPressed: panel.editingDone(); Keys.onEscapePressed: panel.editingDone(); from: 0; to: 59; value: 0; editable: true; Layout.preferredWidth: 92; focusPolicy: Qt.ClickFocus }
            Button { text: "set"; focusPolicy: Qt.NoFocus; onClicked: { watch.setTime(hh.value, mm.value, ss.value); panel.editingDone() } }
        }
        RowLayout {
            spacing: 4
            Label { text: ""; Layout.preferredWidth: 62 }
            Button {
                text: "T := T1 − 5 s"
                focusPolicy: Qt.NoFocus
                onClicked: watch.jumpBeforeAlarm(1, 5)
            }
            Button {
                text: "T := T2 − 5 s"
                focusPolicy: Qt.NoFocus
                onClicked: watch.jumpBeforeAlarm(2, 5)
            }
            Button {
                text: "→ :59:55"
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered; ToolTip.text: "5 s before the next whole hour (chime)"
                onClicked: watch.setTime(watch.hour, 59, 55)
            }
        }
        Label {
            text: "T1 " + two(Math.floor(watch.t1 / 60)) + ":" + two(watch.t1 % 60)
                  + "   T2 " + two(Math.floor(watch.t2 / 60)) + ":" + two(watch.t2 % 60)
                  + "   stopwatch " + (watch.stopwatchRunning ? "running" : "stopped")
            color: "#5a5d63"; font.pixelSize: 11
            Layout.leftMargin: 66
        }

        // ---- input helpers and flags
        RowLayout {
            spacing: 4
            Label { text: "Input"; font.bold: true; Layout.preferredWidth: 62 }
            Button {
                text: buttons.held["b"] && buttons.held["d"] ? "release b+d" : "hold b+d"
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered; ToolTip.text: "beep-test needs b and d held together (keys B and D work too)"
                onClicked: {
                    if (buttons.held["b"] && buttons.held["d"]) { buttons.release("d"); buttons.release("b") }
                    else { buttons.press("b"); buttons.press("d") }
                }
            }
            CheckBox {
                id: anomalies
                text: "Fig. 32 quirk"
                checked: true
                focusPolicy: Qt.NoFocus
                // Show the datamodel's value, however it was set (e.g. --script set:...).
                function refresh() {
                    const v = buttons.dataValue("harelAnomalies")
                    if (v !== undefined && v !== null)
                        checked = v === true
                }
                Component.onCompleted: refresh()
                Connections {
                    target: buttons
                    function onDataValueChanged(name) {
                        if (name === "" || name === "harelAnomalies")
                            anomalies.refresh()
                    }
                }
                ToolTip.visible: hovered; ToolTip.text: "harelAnomalies: pressing a silences the beep-test beep until a is released"
                onToggled: buttons.setDataValue("harelAnomalies", checked)
            }
            CheckBox {
                text: "sound"
                enabled: beeper.available
                checked: beeper.enabled
                focusPolicy: Qt.NoFocus
                onToggled: beeper.enabled = checked
            }
        }

        // ---- event log export
        RowLayout {
            spacing: 4
            Label { text: "Log"; font.bold: true; Layout.preferredWidth: 62 }
            TextField {
                id: path
                Layout.fillWidth: true
                text: "watch-me-events.txt"
                selectByMouse: true
                onAccepted: panel.editingDone()
                Keys.onEscapePressed: panel.editingDone()
            }
            Button {
                text: "export"
                focusPolicy: Qt.NoFocus
                onClicked: { status.text = tracer.exportLog(path.text) ? "saved" : "failed"; panel.editingDone() }
            }
            Button { text: "clear"; focusPolicy: Qt.NoFocus; onClicked: { tracer.clearLog(); panel.editingDone() } }
            Label { id: status; color: "#5a5d63"; font.pixelSize: 11 }
        }
    }
}
