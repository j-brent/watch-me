import QtQuick
import QtQuick.Shapes

// Draws the transitions of the hovered/selected state as arrows over the nested
// boxes (only on demand: arrows across nesting levels would clutter the view).
Item {
    id: overlay
    property var view
    property string focusId: ""
    property int revision: 0          // bump to recompute after layout changes
    enabled: false                    // never steals mouse events

    function centerOf(id) {
        const it = view.registry[id];
        if (!it) return null;
        const p = it.mapToItem(overlay, 0, 0);
        return { x: p.x, y: p.y, w: it.width, h: it.height };
    }
    function edgePoint(r, toward) {
        // point on the rectangle border in the direction of `toward`
        const cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        const dx = toward.x - cx, dy = toward.y - cy;
        if (dx === 0 && dy === 0) return { x: cx, y: r.y };
        const sx = (r.w / 2) / Math.abs(dx || 1e-9), sy = (r.h / 2) / Math.abs(dy || 1e-9);
        const s = Math.min(sx, sy);
        return { x: cx + dx * s, y: cy + dy * s };
    }
    readonly property var arrows: {
        revision;
        if (!view || !focusId) return [];
        const out = [];
        const add = (t, incoming) => {
            const s = centerOf(t.source);
            if (!s) return;
            const lbl = (t.specId ? t.specId + " " : "") + (t.event || "·") + (t.cond ? " [" + t.cond.replace(/In\('([^']+)'\)/g, "in $1") + "]" : "");
            if (!t.targets.length) {
                out.push({ self: true, x: s.x + s.w, y: s.y, label: lbl, incoming: incoming });
                return;
            }
            for (const tg of t.targets) {
                const d = centerOf(tg);
                if (!d) continue;
                const a = edgePoint(s, { x: d.x + d.w / 2, y: d.y + d.h / 2 });
                const b = edgePoint(d, { x: s.x + s.w / 2, y: s.y + s.h / 2 });
                out.push({ self: false, x1: a.x, y1: a.y, x2: b.x, y2: b.y, label: lbl, incoming: incoming });
            }
        };
        for (const t of tree.transitionsFrom(focusId)) add(t, false);
        for (const t of tree.transitionsInto(focusId)) if (t.source !== focusId) add(t, true);
        return out;
    }

    Repeater {
        model: overlay.arrows
        delegate: Item {
            readonly property var a: modelData
            readonly property color col: a.incoming ? "#8a5cc7" : "#2f6fd6"
            readonly property real bulge: a.self ? 0 : 18 + Math.min(40, Math.hypot(a.x2 - a.x1, a.y2 - a.y1) * 0.12)
            readonly property real cx: a.self ? a.x : (a.x1 + a.x2) / 2
            readonly property real cy: a.self ? a.y : (a.y1 + a.y2) / 2 - bulge
            // label position: a point on the curve, staggered so labels of arrows
            // that share an end point do not sit on top of each other
            readonly property real tpos: a.self ? 0 : 0.3 + 0.4 * ((index * 0.618) % 1)
            readonly property real mx: a.self ? a.x : (1 - tpos) * (1 - tpos) * a.x1 + 2 * (1 - tpos) * tpos * cx + tpos * tpos * a.x2
            readonly property real my: a.self ? a.y + 14 * (index % 6) : (1 - tpos) * (1 - tpos) * a.y1 + 2 * (1 - tpos) * tpos * cy + tpos * tpos * a.y2
            readonly property real ang: a.self ? 0 : Math.atan2(a.y2 - cy, a.x2 - cx)
            Shape {
                visible: !a.self
                ShapePath {
                    strokeColor: col; strokeWidth: 2.2; fillColor: "transparent"
                    strokeStyle: a.incoming ? ShapePath.DashLine : ShapePath.SolidLine
                    startX: a.x1 || 0; startY: a.y1 || 0
                    PathQuad { x: a.x2 || 0; y: a.y2 || 0; controlX: cx; controlY: cy }
                }
                ShapePath {   // arrow head
                    strokeColor: col; fillColor: col; strokeWidth: 1
                    startX: a.x2 || 0; startY: a.y2 || 0
                    PathLine { x: (a.x2 || 0) - 10 * Math.cos(ang - 0.4); y: (a.y2 || 0) - 10 * Math.sin(ang - 0.4) }
                    PathLine { x: (a.x2 || 0) - 10 * Math.cos(ang + 0.4); y: (a.y2 || 0) - 10 * Math.sin(ang + 0.4) }
                    PathLine { x: a.x2 || 0; y: a.y2 || 0 }
                }
            }
            Rectangle {
                z: 1
                x: mx - width / 2; y: my - height / 2
                width: lbl.implicitWidth + 8; height: lbl.implicitHeight + 4; radius: 4
                color: "#ffffffee"; border.color: col
                Text { id: lbl; anchors.centerIn: parent; text: (a.self ? "↺ " : "") + a.label; font.pixelSize: 10; color: col }
            }
        }
    }
}
