import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1600
    height: 980
    visible: true
    title: "watch-me — Harel 1987, Fig. 31"
    color: "#f3f1ec"

    // Keys A/B/C/D act as held buttons, so b+d works on the keyboard.
    Item {
        id: keys
        anchors.fill: parent
        focus: true
        readonly property var map: ({ [Qt.Key_A]: "a", [Qt.Key_B]: "b", [Qt.Key_C]: "c", [Qt.Key_D]: "d" })
        Keys.onPressed: (e) => { if (map[e.key] && !e.isAutoRepeat) { buttons.press(map[e.key]); e.accepted = true } }
        Keys.onReleased: (e) => { if (map[e.key] && !e.isAutoRepeat) { buttons.release(map[e.key]); e.accepted = true } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        ColumnLayout {
            Layout.preferredWidth: 480
            Layout.maximumWidth: 480
            Layout.fillHeight: true
            WatchFace { Layout.alignment: Qt.AlignHCenter }
            RowLayout {
                Button {
                    text: tracer.activeSet["dead"] ? "Insert battery" : "Remove battery"
                    focusPolicy: Qt.NoFocus
                    onClicked: buttons.submit(tracer.activeSet["dead"] ? "battery.inserted" : "battery.removed")
                }
            }
            Legend { Layout.fillWidth: true }
            EventLog {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120
            }
        }
        StatechartView {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
