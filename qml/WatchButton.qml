import QtQuick

// A side button of the watch case. Mouse press/release -> a.down / a.up.
Item {
    id: btn
    property string name: "a"
    property bool leftSide: true
    readonly property bool held: buttons.held[name] === true
    width: 26
    height: 34

    Rectangle {
        id: knob
        width: btn.held ? 14 : 22
        height: 30
        anchors.verticalCenter: parent.verticalCenter
        x: btn.leftSide ? btn.width - width : 0
        radius: 4
        color: btn.held ? "#5b5f66" : "#8a8f98"
        border.color: "#2b2e33"
        border.width: 2
        Behavior on width { NumberAnimation { duration: 60 } }
    }
    Text {
        text: btn.name
        font.pixelSize: 20; font.bold: true
        color: "#222"
        anchors.verticalCenter: parent.verticalCenter
        x: btn.leftSide ? -22 : btn.width + 8
    }
    MouseArea {
        anchors.fill: parent
        anchors.margins: -6
        onPressed: buttons.press(btn.name)
        onReleased: buttons.release(btn.name)
        onCanceled: buttons.release(btn.name)
    }
}
