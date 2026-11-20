import QtQuick
import QtQuick.Controls

// Always-on log: inputs (▶) and every transition that fired, by spec row.
Rectangle {
    id: log
    color: "#1e2126"; radius: 6
    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        model: tracer.entries
        onCountChanged: positionViewAtEnd()
        delegate: Text {
            width: list.width
            text: modelData
            font.family: "monospace"; font.pixelSize: 11
            color: modelData.startsWith("▶") ? "#9aa3ad" : "#e8e3d3"
            elide: Text.ElideRight
        }
        ScrollBar.vertical: ScrollBar {}
    }
}
