import QtQuick

// A row of seven-segment digits. `blank` is a list of indices hidden this frame
// (used for blinking fields).
Row {
    id: row
    property string text: ""
    property var blank: []
    property real digitWidth: 30
    property real digitHeight: 52
    property color onColor: "#1b1f1a"
    property color offColor: "#00000000"
    spacing: digitWidth * 0.22
    Repeater {
        model: row.text.length
        delegate: SevenSegment {
            width: row.digitWidth
            height: row.digitHeight
            value: row.blank.indexOf(index) >= 0 ? " " : row.text.charAt(index)
            onColor: row.onColor
            offColor: row.offColor
        }
    }
}
