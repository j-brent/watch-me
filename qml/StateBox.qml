import QtQuick
import QtQuick.Shapes

// One SCXML state, drawn literally nested as in Harel's figures:
//  - compound state: rounded box (solid border) with its children inside
//  - parallel state: its regions side by side, separated by dashed lines
//  - atomic state: compact chip
//  - default child: "●→" glyph; history: circled H / H*
// Fill = active (one meaning only); orange glow = just fired.
Item {
    id: box
    property var node: ({})
    property var view                     // StatechartView (registry, hover, selection, collapse)
    property string parentId: ""
    property string parentKind: ""
    property string parentInitial: ""
    property real regionHeight: 0         // set by a parallel parent so dividers span the row

    readonly property string sid: node.id || ""
    readonly property string kind: node.kind || "atomic"
    readonly property bool isRegion: parentKind === "parallel"
    readonly property bool collapsed: view && view.collapsed[sid] === true
    readonly property var kids: (node.children || []).filter(c => c.kind !== "history")
    readonly property var hist: (node.children || []).filter(c => c.kind === "history")
    readonly property bool active: tracer.activeSet[sid] === true
    readonly property bool isDefault: parentKind !== "parallel" && parentInitial === sid
    readonly property bool focused: view && (view.focusId === sid)
    readonly property int columns: {
        const c = appLayout && appLayout.columns ? appLayout.columns[sid] : undefined;
        if (c !== undefined) return c;
        return kind === "parallel" ? Math.max(1, kids.length) : (kids.length <= 3 ? kids.length : Math.ceil(Math.sqrt(kids.length)));
    }
    readonly property string confidence: {
        const ids = [];
        for (const t of tree.transitionsFrom(sid)) if (t.specId) ids.push(t.specId);
        for (const t of tree.transitionsInto(sid)) if (t.specId) ids.push(t.specId);
        return specTags.weakest(ids);
    }

    function shortName(stateId, ownerId) {
        const cands = [ownerId, ownerId.split("_")[0], "sw", "bt"];
        for (let i = 0; i < cands.length; ++i) {
            const c = cands[i];
            if (c && stateId.startsWith(c + "_") && stateId.length > c.length + 1)
                return stateId.slice(c.length + 1);
        }
        return stateId;
    }
    readonly property string label: shortName(sid, parentId)

    readonly property bool leaf: kids.length === 0 || collapsed
    readonly property real pad: leaf ? 4 : 6
    implicitWidth: Math.max(header.implicitWidth + 2 * pad + (leaf ? 6 : 4), leaf ? 0 : grid.implicitWidth + 2 * pad)
    implicitHeight: leaf ? header.implicitHeight + 2 * pad
                         : header.implicitHeight + pad * 2 + 2 + grid.implicitHeight
    width: implicitWidth
    height: Math.max(implicitHeight, regionHeight)

    Component.onCompleted: if (view) view.registry[sid] = box
    Component.onDestruction: if (view && view.registry[sid] === box) delete view.registry[sid]

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: box.leaf ? 7 : 10
        color: box.active ? (box.leaf ? "#f7c948" : "#fbe7a1") : (box.isRegion ? "transparent" : "#fbfaf7")
        border.width: box.isRegion ? 0 : (box.focused ? 3 : (box.leaf && box.kind !== "compound" && box.kind !== "parallel" ? 1 : 2))
        border.color: box.focused ? "#2f6fd6" : "#2f3237"
        Behavior on color { ColorAnimation { duration: 90 } }
    }

    // flash (~600 ms) when the state took part in the last macrostep
    Rectangle {
        id: flash
        anchors.fill: parent
        anchors.margins: -3
        radius: frame.radius + 3
        color: "transparent"
        border.color: "#ff7a1a"
        border.width: 4
        opacity: 0
        readonly property var stamp: tracer.flashSet[box.sid]
        onStampChanged: if (stamp !== undefined) flashAnim.restart()
        SequentialAnimation {
            id: flashAnim
            NumberAnimation { target: flash; property: "opacity"; to: 1; duration: 60 }
            PauseAnimation { duration: 300 }
            NumberAnimation { target: flash; property: "opacity"; to: 0; duration: 240 }
        }
    }

    Row {
        id: header
        x: box.pad
        y: box.pad - (box.leaf ? 0 : 1)
        spacing: 3
        Text {
            visible: box.isDefault
            text: "●→"
            font.pixelSize: 9
            color: "#2f3237"
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: box.label + (box.collapsed && box.kids.length ? " ⊞" : "")
            font.pixelSize: box.leaf ? 11 : 12
            font.bold: !box.leaf
            font.italic: box.isRegion
            color: "#1d1f22"
        }
        Repeater {
            model: box.hist
            delegate: Rectangle {
                width: 17; height: 17; radius: 9
                color: tracer.activeSet[box.sid] ? "#fff6d5" : "white"
                border.color: "#2f3237"; border.width: 1.5
                anchors.verticalCenter: parent.verticalCenter
                Component.onCompleted: if (box.view) box.view.registry[modelData.id] = this
                Text {
                    anchors.centerIn: parent
                    text: modelData.historyType === "deep" ? "H*" : "H"
                    font.pixelSize: modelData.historyType === "deep" ? 8 : 10
                    font.bold: true
                }
            }
        }
    }

    // spec-confidence badge (corner)
    Rectangle {
        visible: box.confidence !== "" && box.leaf
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 1; anchors.topMargin: 1
        width: 8; height: 8; radius: 4
        color: box.confidence === "P" ? "#3b8f5a" : box.confidence === "F" ? "#d08a1b" : "#c2412d"
        Text { anchors.centerIn: parent; text: box.confidence; color: "white"; font.pixelSize: 6; font.bold: true }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onEntered: if (box.view) box.view.hovered = box.sid
        onExited: if (box.view && box.view.hovered === box.sid) box.view.hovered = box.parentId
        onClicked: if (box.view) box.view.selected = (box.view.selected === box.sid ? "" : box.sid)
        onDoubleClicked: if (box.view && box.kids.length) box.view.toggleCollapsed(box.sid)
    }

    // children as rows (layout.json "rows" or chunks of "columns"); regions of a
    // parallel state are separated by dashed lines, vertical within a row and
    // horizontal between rows.
    readonly property var rowsModel: {
        const hint = appLayout && appLayout.rows ? appLayout.rows[sid] : undefined;
        const byId = {};
        for (const k of kids) byId[k.id] = k;
        const rows = [];
        if (hint) {
            for (const r of hint) rows.push(r.map(id => byId[id]).filter(n => n !== undefined));
        } else {
            for (let i = 0; i < kids.length; i += Math.max(1, columns)) rows.push(kids.slice(i, i + Math.max(1, columns)));
        }
        return rows;
    }
    readonly property real gap: kind === "parallel" ? 14 : 6

    Column {
        id: grid
        visible: !box.leaf
        x: box.pad
        y: header.y + header.implicitHeight + 3
        spacing: box.gap
        Repeater {
            model: box.leaf ? [] : box.rowsModel
            delegate: Item {
                id: rowWrap
                readonly property int rowIndex: index
                width: rowItem.implicitWidth
                height: rowItem.implicitHeight
                // horizontal AND-divider above every row but the first (outside the Row,
                // so it does not take part in the Row's own size)
                Shape {
                    visible: box.kind === "parallel" && rowWrap.rowIndex > 0
                    y: -box.gap / 2
                    width: grid.width; height: 1
                    ShapePath {
                        strokeColor: "#2f3237"; strokeWidth: 1.6
                        strokeStyle: ShapePath.DashLine; dashPattern: [4, 3]
                        fillColor: "transparent"
                        startX: 0; startY: 0
                        PathLine { x: grid.width; y: 0 }
                    }
                }
            Row {
                id: rowItem
                spacing: box.gap
                readonly property real rowH: {
                    if (box.kind !== "parallel") return 0;
                    let h = 0;
                    for (let i = 0; i < children.length; ++i) if (children[i].item) h = Math.max(h, children[i].item.implicitHeight);
                    return h;
                }
                Repeater {
                    model: modelData
                    delegate: Loader {
                        id: ld
                        // initial properties so that every binding sees them from the start
                        Component.onCompleted: setSource("StateBox.qml", {
                            view: box.view, parentId: box.sid, parentKind: box.kind,
                            parentInitial: box.node.initial || "", node: modelData
                        })
                        Binding { target: ld.item; property: "regionHeight"; value: rowItem.rowH; when: ld.item !== null }
                        // vertical AND-divider left of every region but the first in its row
                        Shape {
                            visible: box.kind === "parallel" && index > 0
                            x: -box.gap / 2
                            width: 1; height: ld.height
                            ShapePath {
                                strokeColor: "#2f3237"; strokeWidth: 1.6
                                strokeStyle: ShapePath.DashLine; dashPattern: [4, 3]
                                fillColor: "transparent"
                                startX: 0; startY: 0
                                PathLine { x: 0; y: ld.height }
                            }
                        }
                    }
                }
            }
            }
        }
    }
}
