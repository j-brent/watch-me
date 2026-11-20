import QtQuick

// One SCXML state drawn as a box with its children nested inside.
Rectangle {
    id: box
    property var node: ({})
    readonly property bool active: tracer.activeSet[node.id] === true
    readonly property var kids: (node.children || []).filter(c => c.kind !== "history")

    implicitWidth: Math.max(label.implicitWidth + 16, inner.implicitWidth + 16)
    implicitHeight: label.implicitHeight + 8 + (kids.length ? inner.implicitHeight + 8 : 0)
    width: implicitWidth
    height: implicitHeight
    radius: 8
    color: active ? "#ffe8a3" : "#f6f6f6"
    border.color: "#333"
    border.width: node.kind === "atomic" ? 1 : 2

    Text { id: label; x: 8; y: 4; text: node.id || ""; font.pixelSize: 12 }
    Row {
        id: inner
        x: 8
        y: label.y + label.implicitHeight + 4
        spacing: 8
        Repeater {
            model: box.kids
            delegate: Loader {
                source: "StateBox.qml"
                onLoaded: item.node = modelData
            }
        }
    }
}
