import QtQuick
import QtQuick.Controls

// Live statechart view generated from watch.scxml (ScxmlTreeModel), not hand-drawn.
// Zoom: Ctrl+wheel or the buttons; pan: drag; double-click a state to collapse it.
Item {
    id: view
    objectName: "statechartView"
    clip: true

    property var registry: ({})                 // state id -> StateBox (for arrows)
    property string hovered: ""
    property string selected: ""
    readonly property string focusId: selected !== "" ? selected : hovered
    property var collapsed: {
        const m = {};
        for (const id of (appLayout && appLayout.collapsed) || []) m[id] = true;
        return m;
    }
    function toggleCollapsed(id) {
        const m = Object.assign({}, collapsed);
        if (m[id]) delete m[id]; else m[id] = true;
        collapsed = m;
        relayout();
    }
    property real zoom: 1
    property bool autoFit: true
    function fit() {
        const w = rootBox.implicitWidth + 16, h = rootBox.implicitHeight + 16;
        if (w <= 0 || h <= 0) return;
        zoom = Math.min(flick.width / w, flick.height / h, 2.0);
    }
    function relayout() { relayoutTimer.restart() }
    Timer { id: relayoutTimer; interval: 30; onTriggered: { if (view.autoFit) view.fit(); overlay.revision++ } }
    onWidthChanged: relayout()
    onHeightChanged: relayout()

    Rectangle { anchors.fill: parent; color: "#fdfcf9"; border.color: "#d4d0c6" }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 1
        contentWidth: Math.max(width, (rootBox.implicitWidth + 16) * view.zoom)
        contentHeight: Math.max(height, (rootBox.implicitHeight + 16) * view.zoom)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Item {
            id: content
            width: rootBox.implicitWidth + 16
            height: rootBox.implicitHeight + 16
            scale: view.zoom
            transformOrigin: Item.TopLeft

            StateBox {
                id: rootBox
                x: 8; y: 8
                view: view
                parentKind: "root"
                parentInitial: tree.root.initial || ""
                node: (tree.root.children || [])[0] || ({})
                onImplicitWidthChanged: view.relayout()
                onImplicitHeightChanged: view.relayout()
            }
            TransitionOverlay {
                id: overlay
                anchors.fill: parent
                view: view
                focusId: view.focusId
            }
        }

        WheelHandler {
            acceptedModifiers: Qt.ControlModifier
            onWheel: (e) => {
                view.autoFit = false;
                view.zoom = Math.max(0.2, Math.min(3, view.zoom * (e.angleDelta.y > 0 ? 1.1 : 1 / 1.1)));
                overlay.revision++;
            }
        }
    }

    Legend {
        id: legend
        visible: legendToggle.checked
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 6
    }

    Row {
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 6
        spacing: 4
        Button { id: legendToggle; text: "legend"; checkable: true; checked: true; focusPolicy: Qt.NoFocus }
        Button { text: "−"; width: 30; focusPolicy: Qt.NoFocus; onClicked: { view.autoFit = false; view.zoom /= 1.15 } }
        Button { text: "fit"; width: 40; focusPolicy: Qt.NoFocus; onClicked: { view.autoFit = true; view.fit() } }
        Button { text: "+"; width: 30; focusPolicy: Qt.NoFocus; onClicked: { view.autoFit = false; view.zoom *= 1.15 } }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 6
        text: view.focusId ? "transitions of " + view.focusId + (view.selected ? " (selected — click again to release)" : " (hover)")
                           : "hover or click a state to see its transitions · double-click to collapse · Ctrl+wheel to zoom"
        font.pixelSize: 11; color: "#5a5d63"
    }
}
