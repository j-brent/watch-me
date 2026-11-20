import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1280
    height: 800
    visible: true
    title: "watch-me — Harel 1987, Fig. 31"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        ColumnLayout {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            Button {
                text: tracer.activeSet["dead"] ? "Insert battery" : "Remove battery"
                onClicked: buttons.submit(tracer.activeSet["dead"] ? "battery.inserted" : "battery.removed")
            }
            Item { Layout.fillHeight: true }
        }
        StatechartView {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
