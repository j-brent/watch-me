import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win
    // The host contract (docs/HOST-CONTRACT.md). Every host (cpp/, python/, …)
    // creates this window with these objects; the other QML files reach them by
    // name through this window's context. Main.qml fails to load if one is missing.
    required property QtObject tree       // ScxmlTreeModel
    required property QtObject tracer     // TransitionTracer
    required property QtObject buttons    // ButtonRouter
    required property QtObject watch      // WatchModel
    required property QtObject beeper     // Beeper
    required property QtObject specTags   // SpecTags
    required property var appLayout       // shared/statechart/layout.json, parsed
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
        // A key released while this item has no focus never reaches it: release
        // everything when focus or the window goes away, so no button stays held.
        onActiveFocusChanged: if (!activeFocus) buttons.releaseAll()
    }
    onActiveChanged: if (!active) buttons.releaseAll()
    // Give the keys their focus back: a click on the window background, a watch
    // button pressed with the mouse, or a sim-panel field that is done editing.
    TapHandler { onTapped: keys.forceActiveFocus() }
    Connections {
        target: buttons
        function onHeldChanged() { if (!keys.activeFocus && Object.keys(buttons.held).length > 0) keys.forceActiveFocus() }
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
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: tracer.activeSet["dead"] ? "No battery — press “inserted” below."
                                               : "Mouse: press/hold the side buttons · Keys: hold A B C D"
                color: "#5a5d63"; font.pixelSize: 12
            }
            SimPanel { Layout.fillWidth: true; onEditingDone: keys.forceActiveFocus() }
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
