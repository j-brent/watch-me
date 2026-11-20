import QtQuick
import QtQuick.Shapes

// One seven-segment digit drawn with Qt Quick Shapes (no font assets).
// Segments: a top, b upper right, c lower right, d bottom, e lower left, f upper left, g middle.
Item {
    id: root
    property string value: " "
    property color onColor: "#1b1f1a"
    property color offColor: "#00000000"
    property real thickness: width * 0.18
    property real slant: 0.10                  // italic skew as a fraction of width
    implicitWidth: 30
    implicitHeight: 52

    readonly property var table: ({
        "0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd",
        "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g", " ": "", "_": "d",
        "A": "abcefg", "b": "cdefg", "C": "adef", "c": "deg", "d": "bcdeg", "E": "adefg",
        "F": "aefg", "H": "bcefg", "h": "cefg", "J": "bcde", "L": "def", "n": "ceg", "o": "cdeg",
        "P": "abefg", "r": "eg", "S": "afgcd", "t": "defg", "U": "bcdef", "u": "cde", "y": "bcdfg"
    })
    readonly property string lit: table[value] !== undefined ? table[value] : ""

    // Hexagonal segment polygons in a unit box (w x h), skewed by `slant`.
    function seg(name) {
        const w = width, h = height, t = thickness, g = t * 0.12, m = h / 2;
        let p;
        switch (name) {
        case "a": p = [[g + t/2, 0], [w - g - t/2, 0], [w - g - t, t], [g + t, t], [g + t/2, 0]]; break;
        case "d": p = [[g + t, h - t], [w - g - t, h - t], [w - g - t/2, h], [g + t/2, h], [g + t, h - t]]; break;
        case "g": p = [[g + t, m - t/2], [w - g - t, m - t/2], [w - g - t/2, m], [w - g - t, m + t/2], [g + t, m + t/2], [g + t/2, m], [g + t, m - t/2]]; break;
        case "f": p = [[0, g + t/2], [t, g + t], [t, m - g - t/2], [t/2, m - g], [0, m - g - t/2], [0, g + t/2]]; break;
        case "e": p = [[0, m + g + t/2], [t/2, m + g], [t, m + g + t/2], [t, h - g - t], [0, h - g - t/2], [0, m + g + t/2]]; break;
        case "b": p = [[w, g + t/2], [w, m - g - t/2], [w - t/2, m - g], [w - t, m - g - t/2], [w - t, g + t], [w, g + t/2]]; break;
        case "c": p = [[w, m + g + t/2], [w, h - g - t/2], [w - t, h - g - t], [w - t, m + g + t/2], [w - t/2, m + g], [w, m + g + t/2]]; break;
        }
        return p.map(q => Qt.point(q[0] + slant * w * (1 - q[1] / h), q[1]));
    }

    Repeater {
        model: ["a", "b", "c", "d", "e", "f", "g"]
        delegate: Shape {
            anchors.fill: parent
            ShapePath {
                strokeWidth: -1
                fillColor: root.lit.indexOf(modelData) >= 0 ? root.onColor : root.offColor
                PathPolyline { path: root.width > 0 ? root.seg(modelData) : [] }
            }
        }
    }
}
