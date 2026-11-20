import QtQuick

// Each visual channel of the statechart view has exactly one meaning.
Rectangle {
    id: legend
    implicitWidth: col.implicitWidth + 16
    implicitHeight: col.implicitHeight + 12
    color: "#fdfcf9"; border.color: "#d4d0c6"; radius: 6

    component Swatch: Row {
        property alias text: t.text
        default property alias glyph: holder.data
        spacing: 6
        Item { id: holder; width: 30; height: 16 }
        Text { id: t; font.pixelSize: 11; color: "#2f3237"; anchors.verticalCenter: parent.verticalCenter }
    }

    Column {
        id: col
        x: 8; y: 6
        spacing: 3
        Text { text: "Legend"; font.bold: true; font.pixelSize: 12 }
        Swatch { text: "fill: active (amber) / inactive"
            Rectangle { width: 13; height: 13; radius: 3; color: "#f7c948"; border.color: "#2f3237" }
            Rectangle { x: 16; width: 13; height: 13; radius: 3; color: "#fbfaf7"; border.color: "#2f3237" } }
        Swatch { text: "solid border: state (XOR), boxes nest"
            Rectangle { width: 26; height: 14; radius: 4; color: "transparent"; border.color: "#2f3237"; border.width: 2 } }
        Swatch { text: "dashed line: AND divider (orthogonal regions)"
            Repeater { model: 4; Rectangle { x: 13; y: index * 4; width: 2; height: 3; color: "#2f3237" } } }
        Swatch { text: "●→ default entry ·  H shallow / H* deep history"
            Text { text: "●→ H"; font.pixelSize: 10; font.bold: true } }
        Swatch { text: "orange glow: took part in the last step (~0.6 s)"
            Rectangle { width: 26; height: 14; radius: 4; color: "transparent"; border.color: "#ff7a1a"; border.width: 3 } }
        Swatch { text: "corner badge: spec confidence [P] [F] [A]"
            Rectangle { width: 11; height: 11; radius: 6; color: "#3b8f5a"
                Text { anchors.centerIn: parent; text: "P"; color: "white"; font.pixelSize: 7; font.bold: true } } }
        Swatch { text: "blue arrows: outgoing / purple dashed: incoming"
            Rectangle { y: 6; width: 26; height: 3; color: "#2f6fd6" } }
    }
}
