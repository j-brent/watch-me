import QtQuick

// Watch case + LCD + four buttons. Button placement follows Fig. 7 of the paper
// (a upper left; b, c, d down the right side) and can be changed in layout.json.
Item {
    id: face
    implicitWidth: 470
    implicitHeight: 470

    readonly property var placement: (appLayout && appLayout.buttons) ? appLayout.buttons
        : ({ a: "left-top", b: "right-top", c: "right-middle", d: "right-bottom" })

    // strap
    Rectangle { x: face.width / 2 - 95; y: 0; width: 190; height: face.height; color: "#2c2f33"; radius: 18 }

    Rectangle {
        id: caseBody
        anchors.centerIn: parent
        width: 410; height: 340
        radius: 60
        gradient: Gradient {
            GradientStop { position: 0; color: "#e4e6ea" }
            GradientStop { position: 1; color: "#a7abb3" }
        }
        border.color: "#55595f"; border.width: 3

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 18
            text: "QUARTZ  MULTI-ALARM"
            font.pixelSize: 14; font.bold: true; font.letterSpacing: 2
            color: "#33363b"
        }

        // backlight glow (light.on)
        Rectangle {
            anchors.centerIn: lcd
            width: lcd.width + 26; height: lcd.height + 26; radius: 18
            color: "#6ff0d8"
            opacity: tracer.activeSet["light_on"] ? 0.55 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
        Lcd {
            id: lcd
            anchors.horizontalCenter: parent.horizontalCenter
            y: 56
            width: 330; height: 210
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 18
            text: "a · b · c · d"
            font.pixelSize: 11; color: "#4a4e55"
        }
    }

    function slot(pos) {
        const left = pos.startsWith("left");
        const v = pos.endsWith("top") ? 0.22 : pos.endsWith("middle") ? 0.5 : 0.78;
        return { left: left, y: caseBody.y + caseBody.height * v - 17,
                 x: left ? caseBody.x - 22 : caseBody.x + caseBody.width - 4 };
    }
    Repeater {
        model: ["a", "b", "c", "d"]
        delegate: WatchButton {
            readonly property var s: face.slot(face.placement[modelData] || "right-middle")
            name: modelData
            leftSide: s.left
            x: s.x; y: s.y
        }
    }
}
